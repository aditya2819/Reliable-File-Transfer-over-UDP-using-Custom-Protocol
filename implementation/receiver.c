#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/receiver.h"
#include "include/timer.h"
#include "include/main.h"
#include "include/ops.h"
#include "include/helper.h"
#include "include/consts.h"

// Entry point for file reception logic
void receive_file()
{
	unsigned int curbatch = 0, totalbatch;
	int stop = 0;
	rfileinfo finfo;

	// Prefix path for received file
	memmove(finfo.name, "recv/", 6);

	// Start building context and begin receiving batches
	if(build_rcontext(&finfo))
	{
		totalbatch = CEIL(finfo.size, batchsize);

		if(reqst_firstbatch(&finfo))
		{
			while(curbatch < totalbatch && !stop)
			{
				if(receive_batch(&finfo, curbatch))
					curbatch += 1;
				else
					stop = 1;
			}
		}
	}
	else
		printf("building context failed\n");
}

// Waits for a SENDING message to receive file metadata
int build_rcontext(rfileinfo *finfo)
{
	FILE *fp;
	int success = 0, cur_attempt = 0;
	timer *t = init_timer(60000); // Wait up to 60 seconds

	while(!timer_reached(t) && !success)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);

		if(op == SENDING)
		{
			_saveContext(finfo);
			success = 1;
		}
	}

	destroy_timer(t);
	return success;
}

// Requests the first batch of data (batch 0)
int reqst_firstbatch(rfileinfo *finfo)
{
	int partidx = BTCidx + NUMSIZE;
	int cur_attempt = 0, success = 0;
	timer *t;
	FILE *recvfp;
	unsigned int recv_batch, partno;

	sendbuf[OPidx] = SENDBATCH;
	numtobytes(&(sendbuf[BTCidx]), 0);
	numtobytes(&(sendbuf[partidx]), partsize);
	sendmsglen = 1 + NUMSIZE + NUMSIZE;

	t = init_timer(100);
	send_msg(sendbuf, sendmsglen);

	while(cur_attempt < ATTEMPTS && !success)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);
		recv_batch = bytestonum(&(recvbuf[BTCidx]));
		partno = get_partno(recvbuf[PRTidx]);

		if(op == DATA && recv_batch == 0)
		{
			record_time(t);

			if((recvfp = fopen(finfo->name, "wb+")) != NULL)
			{
				finfo->pd = init_prtdata(PARTSINBATCH);
				writetofile(recvfp, 0);
				set(finfo->pd, partno);
				fclose(recvfp);
				success = 1;
			}
			else
				printf("file creation failed\n");
		}
		else if(timer_reached(t))
		{
			send_msg(sendbuf, sendmsglen);
			reset_timer_offset(t, t->offset * 2); // Exponential backoff
			cur_attempt += 1;
		}
	}

	destroy_timer(t);
	return success;
}

// Requests a specific batch if some parts were missing after recovery
int reqst_batch(rfileinfo *finfo, unsigned int batchno)
{
	int partidx = BTCidx + NUMSIZE;
	int cur_attempt = 0, success = 0;
	timer *t;
	FILE *recvfp;
	unsigned int recv_batch, partno;

	sendbuf[OPidx] = SENDBATCH;
	numtobytes(&(sendbuf[BTCidx]), batchno);
	sendmsglen = 1 + NUMSIZE;

	t = init_timer(MSGLAST);
	send_msg(sendbuf, sendmsglen);

	while(cur_attempt < ATTEMPTS && !success)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);
		recv_batch = bytestonum(&(recvbuf[BTCidx]));
		partno = get_partno(recvbuf[PRTidx]);

		if(op == DATA && recv_batch == batchno)
		{
			record_time(t);

			if((recvfp = fopen(finfo->name, "rb+")) != NULL)
			{
				finfo->pd = init_prtdata(PARTSINBATCH);
				writetofile(recvfp, batchno);
				set(finfo->pd, partno);
				fclose(recvfp);
				success = 1;
			}
			else
				printf("file creation failed\n");
		}
		else if(timer_reached(t))
		{
			send_msg(sendbuf, sendmsglen);
			reset_timer_offset(t, t->offset * 2);
			cur_attempt += 1;
		}
	}

	destroy_timer(t);
	return success;
}

// Writes a received part of a batch to the file at the correct offset
int writetofile(FILE *recvfp, unsigned int batchno)
{
	unsigned char partno = get_partno(recvbuf[PRTidx]);
	unsigned int batchpos = batchno * batchsize;
	unsigned int partpos = batchpos + (partno * partsize);
	unsigned int wrtbyt = 0;

	if(recvfp != NULL)
	{
		fseek(recvfp, partpos, SEEK_SET);
		wrtbyt = fwrite(&(recvbuf[DATAidx]), 1, recvmsglen - 5, recvfp); // 5 = header bytes
		fflush(recvfp);
	}

	return wrtbyt;
}

// Receives all parts of a given batch, then handles recovery if needed
int receive_batch(rfileinfo *finfo, unsigned int curbatch)
{
	int resp = 0, msgcnt = PARTSINBATCH;
	FILE *recvfp;
	unsigned int recved_prtcnt = 0;
	int avg = (MSGMIN + MSGLAST) / 2;
	int fileops = msgcnt;

	timer *t = init_timer(MSGLAST * (msgcnt / 3));
	recvfp = fopen(finfo->name, "rb+");

	while(!timer_reached(t) && recved_prtcnt < PARTSINBATCH && recvfp != NULL)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);
		unsigned int partno = get_partno(recvbuf[OPidx]);
		unsigned int batchno = bytestonum(&(recvbuf[BTCidx]));

		if(recvmsglen > 0 && op == DATA && batchno == curbatch)
		{
			writetofile(recvfp, curbatch);
			set(finfo->pd, partno);
			reinit_timer(t, avg * 5 + fileops);
			recved_prtcnt += 1;
		}
	}

	fclose(recvfp);
	destroy_timer(t);
	resp = recover_parts(finfo, curbatch); // Try to get any missing parts
	return resp;
}

// Builds a message to request missing parts in a batch
unsigned int create_missing_part_request(unsigned char *part_arr, unsigned int arrsize, unsigned int batchno)
{
	int buflimit = partsize, i;
	sendbuf[OPidx] = RESENDPARTS;
	numtobytes(&(sendbuf[BTCidx]), batchno);

	for(i = 0; i < arrsize && i < buflimit; i++)
		sendbuf[DATAidx + i] = part_arr[i];

	return (i + DATAidx);
}

// Recovers missing parts from a batch using RESENDPARTS request/response
int recover_parts(rfileinfo *finfo, unsigned int curbatch)
{
	int resp = 0, missprt_cnt = 0, tryno = 0;
	unsigned char missparts_arr[PARTSINBATCH];
	unsigned int partno, batchno;

	timer *t = init_timer(MSGLAST * (PARTSINBATCH / 10));
	FILE *recvfp = fopen(finfo->name, "rb+");

	missprt_cnt = getmisprts(finfo->pd, missparts_arr);

	while(missprt_cnt > 0 && tryno < ATTEMPTS && recvfp != NULL)
	{
		recvmsglen = recv_msg(recvbuf);
		op = get_op(recvbuf[OPidx]);
		partno = get_partno(recvbuf[OPidx]);
		batchno = bytestonum(&(recvbuf[BTCidx]));

		if(recvmsglen > 0 && op == DATA && batchno == curbatch)
		{
			writetofile(recvfp, curbatch);
			set(finfo->pd, partno);
			reinit_timer(t, MSGLAST * (PARTSINBATCH / 10));
			tryno = 0;
		}

		missprt_cnt = getmisprts(finfo->pd, missparts_arr);

		if(timer_reached(t) && missprt_cnt > 0)
		{
			sendmsglen = create_missing_part_request(missparts_arr, missprt_cnt, curbatch);
			send_msg(sendbuf, sendmsglen);
			tryno += 1;
			reinit_timer(t, MSGLAST * (PARTSINBATCH / 2) + PARTSINBATCH);
		}
	}

	if(missprt_cnt == 0)
	{
		printf("%u received\n", curbatch);
		destroy_prddata(finfo->pd);
		resp = reqst_batch(finfo, curbatch + 1);
	}

	destroy_timer(t);
	if(recvfp != NULL) fclose(recvfp);
	return resp;
}

// Extracts metadata from the initial SENDING message and adjusts parameters
void _saveContext(rfileinfo *finfo)
{
	int partidx, sizeidx;
	unsigned int s_partsize;

	sizeidx = FLidx + FILENMLEN;
	partidx = sizeidx + NUMSIZE;

	memmove(&(finfo->name[5]), &(recvbuf[FLidx]), FILENMLEN);
	finfo->size = bytestonum(&(recvbuf[sizeidx]));
	s_partsize = bytestonum(&(recvbuf[partidx]));

	if(s_partsize < partsize)
	{
		partsize = s_partsize;
		batchsize = partsize * PARTSINBATCH;
	}
}
