#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <stdint.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "include/helper.h"
#include "include/consts.h"
#include "include/ops.h"

#define _BSD_SOURCE

/**
 * Checks if the provided string is a valid IPv4 address.
 * Returns 1 if valid, 0 otherwise.
 */
int isvalid_addr(char *addr)
{
	struct in_addr *inp;
	int res;
	res = inet_aton(addr, inp);
	return (res > 0) ? 1 : 0;
}

/**
 * Checks if a given file exists and has read and write permissions.
 * Returns 1 if accessible, 0 otherwise.
 */
int isvalid_file(char *filenm)
{
	int res;
	res = access(filenm, F_OK | R_OK | W_OK);
	return (res == 0) ? 1 : 0;
}

/**
 * Returns the size (in bytes) of the given file.
 * Returns -1 if the file cannot be accessed or read.
 */
int get_filesize(char *filenm)
{
	FILE *fp;
	int size = -1;

	fp = fopen(filenm, "rb+");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fclose(fp);
	}
	else
	{
		printf("err while calculating file size : %s\nfilename %s\n", strerror(errno), filenm);
	}
	return size;
}

/**
 * Converts a 4-byte array (little-endian) into an unsigned integer.
 */
unsigned int bytestonum(unsigned char *arr)
{
	unsigned int num = 0, n;
	for (int i = 0; i < 4; i++)
	{
		num = (num << 8) | arr[i];
	}
	n = (unsigned int)le32toh((uint32_t)num);
	return n;
}

/**
 * Converts an unsigned integer into a 4-byte array using little-endian format.
 */
void numtobytes(unsigned char *buffer, unsigned int n)
{
	unsigned int num = (unsigned int)htole32((uint32_t)n);
	for (int i = 0; i < 4; i++)
	{
		buffer[i] = (num & 0xFF000000) >> 24;
		num <<= 8;
	}
}

/**
 * Computes the ceiling of a divided by b for positive integers.
 * Returns -1 if a is 0.
 */
int CEIL(int a, int b)
{
	int res = -1;
	if (a != 0)
	{
		res = a / b;
		res += (a % b == 0) ? 0 : 1;
	}
	return res;
}

/**
 * Encodes the part number into a byte for a DATA message.
 * Sets the MSB to 1 and keeps lower 7 bits for the part number.
 */
unsigned char encode_part(unsigned char part)
{
	part |= (1 << 7);
	return part;
}

/**
 * Extracts the part number from a DATA operation byte.
 * Clears the MSB to get the original 7-bit part number.
 */
unsigned char get_partno(unsigned char ch)
{
	ch = (ch << 1) >> 1;
	return ch;
}

/**
 * Determines the message operation type from the byte.
 * If MSB is set, it's a DATA message. Otherwise, the last 3 bits represent the operation.
 */
unsigned char get_op(unsigned char ch)
{
	unsigned char mask = 1 << 7, op;
	if ((ch & mask) == mask)
		op = DATA;
	else
		op = (ch & 7);
	return op;
}