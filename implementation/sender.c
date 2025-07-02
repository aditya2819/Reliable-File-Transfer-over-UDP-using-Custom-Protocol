#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/sender.h"
#include "include/timer.h"
#include "include/main.h"
#include "include/ops.h"
#include "include/helper.h"
#include "include/consts.h"

// Handles sending the file in batches
void send_file(char *filename)
{
	int stop = 0;
	unsigned int curbatch = 0, totalbatch;
	sfileinfo finfo;

	// Copy filename into file info struct
	memmove(finfo.name, filename, FILENMLEN);
	finfo.size = get_filesize(filename);

	// Calculate total number of batches
	totalbatch = CEIL(finfo.size, batchsize);

	// Begin sending if context is successfully shared
	if(build_scontext(&finfo) == 1)
	{
		while((curbatch < totalbatch) && (stop == 0))
		{
			if(send_batch(&finfo, curbatch))
				curbatch += 1;
			else
				stop = 1;
		}
	}
	else
	{
		printf("Failed to build context\n");
	}
}

// Builds and sends the initial context message to the receiver
// Retries up to 3 times if acknowledgment is not received
int build_scontext(sfileinfo *finfo)
{
	int success = 0;
	int cur_attempt = 0, totalattempts = 3;
	unsigned int reqst_batch, expd_batch = 0;

	_sendContext(finfo);

	timer *t = init_timer(100); // 10-second timeout

	while(cur_attempt < totalattempts && success == 0)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]); 
		reqst_batch = bytestonum(&(recvbuf[BTCidx]));

		if(recvmsglen > 0 && op == SENDBATCH && reqst_batch == 0)
		{
			record_time(t);
			_adjustContext();
			success = 1;
		}
		else if(timer_reached(t))
		{
			cur_attempt += 1;
			send_msg(sendbuf, sendmsglen);
			reset_timer_offset(t, t->offset * 2); // exponential backoff
		}
	}

	destroy_timer(t);
	return success;
}

// Sends a batch of parts of the file corresponding to the given batch number
int send_batch(sfileinfo *finfo, unsigned int curbatch)
{
	int resp = 0;
	unsigned char partno = 0;
	FILE *sendfp = fopen(finfo->name, "rb");
	unsigned int batchpos = curbatch * batchsize;

	if(sendfp != NULL)
	{
		fseek(sendfp, batchpos, SEEK_SET);

		while(!feof(sendfp) && partno < PARTSINBATCH)
		{
			sendbuf[OPidx] = encode_part(partno);
			numtobytes(&(sendbuf[BTCidx]), curbatch);
			sendmsglen = 1 + NUMSIZE;
			sendmsglen += fread(&(sendbuf[DATAidx]), 1, partsize, sendfp);
			send_msg(sendbuf, sendmsglen);
			partno += 1;
		}
		
		fclose(sendfp);

		// Handle retransmission of missing parts if requested
		resp = send_missing_parts(finfo, curbatch);
	}
	else 
	{
		printf("Unable to load the file\n");
	}

	return resp;
}

// Handles retransmission of missing parts requested by the receiver
int send_missing_parts(sfileinfo *finfo, unsigned int curbatch)
{
	int resp = 0, tryno = 0, msgcnt = PARTSINBATCH, atmpt = 0;
	unsigned char partno;
	unsigned int reqst_batchno, batchpos, partpos;
	int fileops = msgcnt * 1;
	timer *t = init_timer(MSGMAX * msgcnt + fileops);
	FILE *sendfp = fopen(finfo->name, "rb");

	batchpos = curbatch * batchsize;

	while(atmpt < ATTEMPTS && resp == 0)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);
		reqst_batchno = bytestonum(&(recvbuf[BTCidx]));

		// Successful transmission confirmed
		if(recvmsglen > 0 && op == SENDBATCH && reqst_batchno == (curbatch + 1))
		{
			printf("%u sent\n", curbatch);
			resp = 1;
		}
		// Retransmit specific parts requested by receiver
		else if(recvmsglen > 0 && op == RESENDPARTS && reqst_batchno == curbatch && sendfp != NULL)
		{
			printf("Sending missing parts for batch %d: ", curbatch);

			for(int i = DATAidx; i < recvmsglen; i++)
			{
				partno = recvbuf[i];
				printf("%d ", partno);
				partpos = batchpos + (partno * partsize);
				fseek(sendfp, partpos, SEEK_SET);

				sendbuf[OPidx] = encode_part(partno);
				numtobytes(&(sendbuf[BTCidx]), curbatch);
				sendmsglen = 1 + NUMSIZE;
				sendmsglen += fread(&(sendbuf[DATAidx]), 1, partsize, sendfp);
				send_msg(sendbuf, sendmsglen);
			}
			reset_timer(t);
			printf("\n");
		}
		// Timeout occurred, retry after increasing timeout window
		else if(timer_reached(t))
		{
			reset_timer_offset(t, t->offset * 2);
			atmpt += 1;
		}
	}

	destroy_timer(t);
	if(sendfp != NULL) fclose(sendfp);
	return resp;
}

// Constructs and sends the initial context message with file info
void _sendContext(sfileinfo *finfo)
{
	int prtidx, sizeidx;

	sizeidx = FLidx + FILENMLEN;
	prtidx = sizeidx + NUMSIZE;

	sendbuf[OPidx] = SENDING;
	memmove(&(sendbuf[FLidx]), finfo->name, FILENMLEN);
	numtobytes(&(sendbuf[sizeidx]), finfo->size);
	numtobytes(&(sendbuf[prtidx]), partsize);

	sendmsglen = 1 + FILENMLEN + NUMSIZE + NUMSIZE;
	send_msg(sendbuf, sendmsglen);
}

// Adjusts context if receiver requests different part size
void _adjustContext()
{
	unsigned int r_partsize;
	int partidx = BTCidx + NUMSIZE;

	r_partsize = bytestonum(&(recvbuf[partidx]));
	if(partsize != r_partsize)
	{
		partsize = r_partsize;
		batchsize = partsize * PARTSINBATCH;
	}
}
