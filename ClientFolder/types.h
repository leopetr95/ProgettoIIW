#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DIMENSION 512


typedef struct Packet{

	char data[DIMENSION];
	int type;
	int seq_num;
	struct timespec time;

}Header;