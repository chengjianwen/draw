#ifndef __LIBFIFOH__
#define __LIBFIFOH__

#include <stdint.h>
#define SIZE_FIFO 64
typedef struct fifo
{
	int start;
	int stop;
	void *mem[SIZE_FIFO+1];
	struct fifo * nextFifo;
	int taskId;
} fifo;

int writeFifo(fifo * wFifo, void *data );
int readFifo(fifo * rFifo, void ** data );
#endif
