#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atx.h"

int main (int argc, char **argv) {

	FILE* file;
	unsigned char t = 1;

	if(argc < 2) return(1);

	file = fopen(argv[1], "r");
	if(loadAtxFile(file)) {
		printf("no atx file\n");
		fclose(file);
		return(1);
	}

	while(getAtxTrack(t)) t++;

	closeAtxFile();
	fclose(file);

	return(0);
}
