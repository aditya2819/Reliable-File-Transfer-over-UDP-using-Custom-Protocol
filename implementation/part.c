#include <stdio.h>
#include <stdlib.h>

#include "include/part.h"
#include "include/helper.h"

#define GRPTYPE unsigned int                 // Data type for each group (32-bit)
#define GRPSIZE (sizeof(GRPTYPE) * 8)        // Number of bits in a group (typically 32)

/**
 * Initializes a new `prtdata` structure to track parts using bitmaps.
 * Each bit in an integer indicates the presence (1) or absence (0) of a part.
 */
struct prtdata *init_prtdata(int size)
{
	struct prtdata *pd;
	int nprt;

	pd = (struct prtdata *)malloc(sizeof(struct prtdata));

	// Calculate how many integers (groups) are needed to store all parts
	nprt = CEIL(size, GRPSIZE);
	pd->n = nprt;
	pd->mem = (GRPTYPE *)malloc(sizeof(GRPTYPE) * nprt);
	pd->expcnt = size;

	// Initialize all bits to 0 (no parts received yet)
	for (int i = 0; i < pd->n; i++)
		pd->mem[i] = 0;

	return pd;
}

/**
 * Frees memory allocated for the prtdata structure.
 */
void destroy_prddata(struct prtdata *pd)
{
	free(pd->mem);
	free(pd);
}

/**
 * Marks a part as received by setting the bit at the given position.
 */
void set(struct prtdata *pd, unsigned int pos)
{
	unsigned int grpno, grppos;

	if (pd != NULL)
	{
		grpno = pos / GRPSIZE;
		grppos = pos % GRPSIZE;
		pd->mem[grpno] |= (1 << grppos);
	}
}

/**
 * Resets (clears) the bit corresponding to a part, marking it as not received.
 */
void reset(struct prtdata *pd, unsigned int pos)
{
	unsigned int grpno, grppos;

	if (pd != NULL)
	{
		grpno = pos / GRPSIZE;
		grppos = pos % GRPSIZE;
		pd->mem[grpno] &= ~(1 << grppos);
	}
}

/**
 * Checks if a specific part has been received.
 * Returns non-zero if present, 0 if not present.
 */
int ispart_present(struct prtdata *pd, unsigned int pos)
{
	int resp = -1;
	unsigned int grpno, grppos;

	if (pd != NULL)
	{
		grpno = pos / GRPSIZE;
		grppos = pos % GRPSIZE;
		resp = pd->mem[grpno] & (1 << grppos);
	}
	return resp;
}

/**
 * Scans through all parts and populates the array with missing part indices.
 * Returns the total count of missing parts.
 */
unsigned int getmisprts(struct prtdata *pd, unsigned char *arr)
{
	unsigned int i = 0, j = 0, k = 0;

	// Check full groups
	while (i < pd->n - 1)
	{
		if (pd->mem[i] != 0xFFFFFFFF)  // Only process groups with missing parts
		{
			while (j < GRPSIZE)
			{
				if ((pd->mem[i] & (1 << j)) == 0)
					arr[k++] = (GRPSIZE * i) + j;
				j++;
			}
		}
		i++;
		j = 0;
	}

	// Check the last group (which may be partially filled)
	int limit = (pd->expcnt % GRPSIZE);
	while (j < limit)
	{
		if ((pd->mem[i] & (1 << j)) == 0)
			arr[k++] = (GRPSIZE * i) + j;
		j++;
	}

	return k;
}

/**
 * Prints all part indices that have been marked as received.
 */
void print_values(struct prtdata *pd)
{
	int i;

	// Print full groups
	for (i = 0; i < pd->n - 1; i++)
	{
		for (int j = 0; j < GRPSIZE; j++)
		{
			if ((pd->mem[i] & (1 << j)) != 0)
				printf("%ld ", ((i * GRPSIZE) + j));
		}
	}

	// Print last group (may not be fully filled)
	int last = (pd->expcnt % GRPSIZE == 0) ? GRPSIZE : (pd->expcnt % GRPSIZE);
	for (int j = 0; j < last; j++)
	{
		if ((pd->mem[i] & (1 << j)) != 0)
			printf("%ld ", ((i * GRPSIZE) + j));
	}

	printf("\n");
}
