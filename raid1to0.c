#include <stdio.h>
#include <stdlib.h>

#include "lib1to0.h"

#define DEFAULT_CHUNK_SIZE (524288) /* 512 KB, seems to be the default. */

void statusoutput(unsigned long int cur, unsigned long int max);

int main(int argc, char **argv) {
	mddevice** devices;
	int devcount;
	int retval;
	int i;

	if(argc < 3) {
		fprintf(stderr, "You must provide at least 2 devices for conversion.\n");
		exit(-1);
	}
	if(argc > MD_SB_DISKS) {
		fprintf(stderr, "Linux RAID only supports up to MD_SB_DISKS disks in an array.\n");
		exit(-1);
	}
	devcount = argc - 1;
	devices = malloc(sizeof(mddevice *) * devcount);
	if(devices == NULL) {
		exit(-3);
	}

	for(i = 0; i < devcount; i++) {
		devices[i] = openmddev(argv[i + 1]);
		if(devices[i] == NULL) {
			perror(argv[i + 1]);
			exit(-2);
		}
		dumpmddev(devices[i]);
	}

	retval = verifymddevs(devices, devcount);
	if(retval != R10_SUCCESS) {
		fprintf(stderr, "Verification failed: ");
		raid1to0_perror(retval);
		fprintf(stderr, "\n");
		exit(-4);
	}

	retval = reordermddevs(devices, devcount);
	if(retval != R10_SUCCESS) {
		fprintf(stderr, "Sorting failed: ");
		raid1to0_perror(retval);
		fprintf(stderr, "\n");
		exit(-5);
	}

	retval = restripe(devices, devcount, DEFAULT_CHUNK_SIZE, statusoutput);
	if(retval != R10_SUCCESS) {
		fprintf(stderr, "Striping failed: ");
		if(retval == R10_EGENERIC)
			perror("restripe");
		else
			raid1to0_perror(retval);
		fprintf(stderr, "\n");
		exit(-6);
	}
	fprintf(stderr, "\ncomplete\n");

	for(i = 0; i < devcount; i++) {
		closemddev(devices[i]);
	}

	exit(0);
}

void statusoutput(unsigned long int cur, unsigned long int max) {
	fprintf(stderr, "%lu/%lu\r", cur, max);
}
