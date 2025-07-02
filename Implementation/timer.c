#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "intr/timer.h"

// Global variables to track message timing stats
timer *__t;
long long MSGMIN = -1, MSGMAX = -1, MSGLAST = -1;

// Initializes a timer with the specified offset (in milliseconds)
timer *init_timer(long offset)
{
	timer *t = (timer *)malloc(sizeof(timer));
	clock_gettime(CLOCK_MONOTONIC, &(t->basetime));
	t->offset = offset;
	return t;
}

// Doubles the timer's offset to simulate exponential backoff
void reset_timer(timer *t)
{
	t->offset += t->offset;
}

// Reinitializes the timer with a new base time and offset
void reinit_timer(timer *t, long offset)
{
	clock_gettime(CLOCK_MONOTONIC, &(t->basetime));
	t->offset = offset;
}

// Updates the timer by increasing the offset (used after timeout)
void reset_timer_offset(timer *t, long offset)
{
	t->offset += offset;
}

// Checks if the timer has expired (current time exceeds base + offset)
int timer_reached(timer *t)
{
	long long ref_milisec, cur_milisec;
	struct timespec curtime;

	clock_gettime(CLOCK_MONOTONIC, &curtime);

	ref_milisec = (t->basetime.tv_sec * 1000) + (t->basetime.tv_nsec / 1000000);
	cur_milisec = (curtime.tv_sec * 1000) + (curtime.tv_nsec / 1000000);

	return ((ref_milisec + t->offset) < cur_milisec);
}

// Frees memory allocated for the timer
void destroy_timer(timer *t)
{
	free(t);
}

// Prints the current time in nanoseconds since system boot (monotonic clock)
void cur_nanosec()
{
	long long cur_milisec;
	struct timespec curtime;

	clock_gettime(CLOCK_MONOTONIC, &curtime);

	printf("time :%lld\n", ((curtime.tv_sec * 1000000000LL) + curtime.tv_nsec));
}

// Records the time elapsed since the timer was started, updating MSGMIN, MSGMAX, and MSGLAST
void record_time(timer *t)
{
	long long cur_milisec, ref_milisec, newtime;
	struct timespec curtime;

	clock_gettime(CLOCK_MONOTONIC, &curtime);

	ref_milisec = (t->basetime.tv_sec * 1000) + (t->basetime.tv_nsec / 1000000);
	cur_milisec = (curtime.tv_sec * 1000) + (curtime.tv_nsec / 1000000);

	newtime = cur_milisec - ref_milisec;
	if (newtime <= 0) newtime = 1;

	if (MSGLAST == -1)
	{
		MSGMIN = newtime;
		MSGMAX = newtime;
	}
	else
	{
		if (newtime < MSGMIN)
			MSGMIN = newtime;

		if (newtime > MSGMAX)
			MSGMAX = newtime;
	}

	MSGLAST = newtime;
}

// Prints the tracked min, max, and last message response times
void print_reftime()
{
	printf("MSGMIN %lld\n", MSGMIN);
	printf("MSGMAX %lld\n", MSGMAX);
	printf("MSGLAST %lld\n", MSGLAST);
}