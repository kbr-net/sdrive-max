#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atx.h"

int main (int argc, char **argv) {

	FILE* file;
	unsigned char t;

	if(argc < 2) return(1);

	file = fopen(argv[1], "r");
	if(loadAtxFile(file)) {
		printf("no atx file\n");
		fclose(file);
		return(1);
	}

	for(t = 1; t <= 40; t++) {
		getAtxTrack(t);
	}

	closeAtxFile();
	fclose(file);

	return(0);
}
