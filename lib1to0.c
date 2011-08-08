#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib1to0.h"

inline long unsigned int sbposition(unsigned long int size) {
	/* taken from mdadm's md_p.h */
	return((size & ~(MD_RESERVED_BYTES - 1)) - MD_RESERVED_BYTES);
}

unsigned int checksum(unsigned int *data, unsigned int length) {
	/* rewritten based on from mdadm's util.c */
	unsigned long int current = 0;
	unsigned int checksum;
	int i;

	for(i = 0; i < length; i++)
		current += data[i];
	return((current & 0xFFFFFFFF) + (current >> 32));
}

unsigned int sbcsum(mdp_super_t *sb) {
	unsigned int chsum;
	unsigned int newcsum;

	chsum = sb->sb_csum;
	sb->sb_csum = 0;
	newcsum = checksum((unsigned int *)sb, MD_SB_WORDS);
	sb->sb_csum = chsum;

	return(newcsum);
}

mddevice *openmddev(const char *pathname) {
	mddevice *device;

	device = malloc(sizeof(mddevice));
	if(device == NULL) {
		goto error1;
	}

	device->path = malloc(sizeof(char) * (strlen(pathname) + 1));
	if(device->path == NULL) {
		goto error2;
	}
	strcpy(device->path, pathname);

	device->fd = open(device->path, O_RDWR);
	if(device->fd == -1) {
		goto error3;
	}

	if(ioctl(device->fd, BLKGETSIZE64, &(device->size)) == -1) {
		goto error3;
	}

	device->sbpos = sbposition(device->size);
	if(lseek(device->fd, device->sbpos, SEEK_SET) != device->sbpos) {
		goto error3;
	}
	if(read(device->fd, &(device->superblock), sizeof(mdp_super_t)) != sizeof(mdp_super_t)) {
		goto error3;
	}

	return(device);

error3:
	free(device->path);
error2:
	free(device);
error1:
	return(NULL);
}

void dumpmddev(mddevice *device) {
	long unsigned int t;
	unsigned int i;

	fprintf(stderr, "Name %s, size %lu, superblock at %lu\n",
		device->path, device->size, device->sbpos);
	fprintf(stderr, "***Constant Generic Information***\n");
	fprintf(stderr, " Superblock magic %X\n", device->superblock.md_magic);
	fprintf(stderr, " Superblock version %u.%u.%u\n",
		device->superblock.major_version,
		device->superblock.minor_version,
		device->superblock.patch_version);
	fprintf(stderr, " Generic section used words %u\n", device->superblock.gvalid_words);
	fprintf(stderr, " UUID %X-%X-%X-%X\n",
		device->superblock.set_uuid0,
		device->superblock.set_uuid1,
		device->superblock.set_uuid2,
		device->superblock.set_uuid3);
	fprintf(stderr, " Creation time %u\n", device->superblock.ctime);
	fprintf(stderr, " RAID level (personality) %u\n", device->superblock.level);
	fprintf(stderr, " Size in blocks, bytes %u, %lu\n",
		device->superblock.size,
		(unsigned long int)device->superblock.size * 1024);
	fprintf(stderr, " Total disks in set (including spares) %u\n", device->superblock.nr_disks);
	fprintf(stderr, " RAID disks in set %u\n", device->superblock.raid_disks);
	fprintf(stderr, " Preferred MD dev minor number %u\n", device->superblock.md_minor);
	if(device->superblock.not_persistent) {
		fprintf(stderr, " Superblock is not persistent.\n");
	} else {
		fprintf(stderr, " Superblock is persistent.\n");
	}
	fprintf(stderr, "***Generic State Information***\n");
	fprintf(stderr, " Superblock update time %u\n", device->superblock.utime);
	fprintf(stderr, " Superblock state %X", device->superblock.state);
	if(device->superblock.state & (1 << MD_SB_CLEAN))
		fprintf(stderr, " CLEAN");
	if(device->superblock.state & (1 << MD_SB_ERRORS))
		fprintf(stderr, " ERRORS");
	if(device->superblock.state & (1 << MD_SB_BITMAP_PRESENT))
		fprintf(stderr, " BITMAP");
	fprintf(stderr, "\n");
	fprintf(stderr, " Disks currently active, working, failed, spare %u, %u, %u, %u\n",
		device->superblock.active_disks,
		device->superblock.working_disks,
		device->superblock.failed_disks,
		device->superblock.spare_disks);
	fprintf(stderr, " Superblock checksum %X", device->superblock.sb_csum);
	i = sbcsum(&(device->superblock));
	if(device->superblock.sb_csum == i) {
		fprintf(stderr, " GOOD\n");
	} else {
		fprintf(stderr, " BAD (%X)\n", i);
	}
	t = (unsigned long int)(device->superblock.events_lo) | ((unsigned long int)(device->superblock.events_hi) << 32);
	fprintf(stderr, " Superblock update count %lu\n", t);
	t = (unsigned long int)(device->superblock.cp_events_lo) | ((unsigned long int)(device->superblock.cp_events_hi) << 32);
	fprintf(stderr, " Checkpoint update count %lu\n", t);
	fprintf(stderr, " Recovery checkpoint sector count %u\n", device->superblock.recovery_cp);
	fprintf(stderr, "***Personality Information***\n");
	fprintf(stderr, " Physical layout %X\n", device->superblock.layout);
	fprintf(stderr, " Chunk size %u\n", device->superblock.chunk_size);
	fprintf(stderr, " Root PV %u\n", device->superblock.root_pv);
	fprintf(stderr, " Root block %u\n", device->superblock.root_block);
	fprintf(stderr, "***Disks Information***\n");
	for(i = 0; i < device->superblock.raid_disks; i++) {
		fprintf(stderr, " Disk %u", i);
		if(device->superblock.this_disk.number == i)
			fprintf(stderr, " <");
		fprintf(stderr, "\n");
		fprintf(stderr, "  Number %u\n", device->superblock.disks[i].number);
		fprintf(stderr, "  Device major number %u\n", device->superblock.disks[i].major);
		fprintf(stderr, "  Device minor number %u\n", device->superblock.disks[i].minor);
		fprintf(stderr, "  Role number %u\n", device->superblock.disks[i].raid_disk);
		fprintf(stderr, "  State %u", device->superblock.disks[i].state);
		if(device->superblock.disks[i].state & (1 << MD_DISK_FAULTY))
			fprintf(stderr, " FAULTY");
		if(device->superblock.disks[i].state & (1 << MD_DISK_ACTIVE))
			fprintf(stderr, " ACTIVE");
		if(device->superblock.disks[i].state & (1 << MD_DISK_SYNC))
			fprintf(stderr, " SYNC");
		if(device->superblock.disks[i].state & (1 << MD_DISK_REMOVED))
			fprintf(stderr, " REMOVED");
		if(device->superblock.disks[i].state & (1 << MD_DISK_WRITEMOSTLY))
			fprintf(stderr, " WRITEMOSTLY");
		fprintf(stderr, "\n");
	}
	fprintf(stderr, " This Disk\n");
	fprintf(stderr, "  Number %u\n", device->superblock.this_disk.number);
	fprintf(stderr, "  Device major number %u\n", device->superblock.this_disk.major);
	fprintf(stderr, "  Device minor number %u\n", device->superblock.this_disk.minor);
	fprintf(stderr, "  Role number %u\n", device->superblock.this_disk.raid_disk);
	fprintf(stderr, "  State %u", device->superblock.this_disk.state);
	if(device->superblock.this_disk.state & (1 << MD_DISK_FAULTY))
		fprintf(stderr, " FAULTY");
	if(device->superblock.this_disk.state & (1 << MD_DISK_ACTIVE))
		fprintf(stderr, " ACTIVE");
	if(device->superblock.this_disk.state & (1 << MD_DISK_SYNC))
		fprintf(stderr, " SYNC");
	if(device->superblock.this_disk.state & (1 << MD_DISK_REMOVED))
		fprintf(stderr, " REMOVED");
	if(device->superblock.this_disk.state & (1 << MD_DISK_WRITEMOSTLY))
		fprintf(stderr, " WRITEMOSTLY");
	fprintf(stderr, "\n");
}

raid1to0_error verifymddevs(mddevice **devices, int devcount) {
	int i, j;
	/* Individual sanity checks */
	for(i = 0; i < devcount; i++) {
		if(devices[i]->superblock.md_magic != MD_SB_MAGIC)
			return(R10_EMAGIC);
		if(devices[i]->superblock.major_version != 0 ||
			devices[i]->superblock.minor_version != 90)
			return(R10_EVERSION);
		if(devices[i]->superblock.sb_csum != sbcsum(&(devices[i]->superblock)))
			return(R10_ECHECKSUM);
		if(devices[i]->superblock.size * 1024 > devices[i]->size)
			return(R10_ESIZE);
		if(devices[i]->superblock.level != 1) 
			return(R10_ELEVEL);
		if(devices[i]->superblock.nr_disks != devices[i]->superblock.raid_disks)
			return(R10_EFEATURE);
		if(devices[i]->superblock.raid_disks != devcount)
			return(R10_EDISK);
		if(devices[i]->superblock.not_persistent == 1)
			return(R10_EFEATURE);
		if(devices[i]->superblock.state != 1)
			return(R10_ECLEAN);
		if(devices[i]->superblock.active_disks != devices[i]->superblock.raid_disks)
			return(R10_ECLEAN);
		if(devices[i]->superblock.working_disks != devices[i]->superblock.raid_disks)
			return(R10_ECLEAN);
		if(devices[i]->superblock.failed_disks != 0)
			return(R10_ECLEAN);
		if(devices[i]->superblock.spare_disks != 0)
			return(R10_EFEATURE);
	}

	/* group sanity checks */
	for(i = 0; i < devcount - 1; i++) {
		for(j = i + 1; j < devcount; j++) { /* don't recompare the same values */
			/* group sanity checks against all */
			if(strcmp(devices[i]->path, devices[j]->path) == 0)
				return(R10_EDUPLICATE);
		}
		if(devices[i]->superblock.set_uuid0 != devices[i + 1]->superblock.set_uuid0)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.set_uuid1 != devices[i + 1]->superblock.set_uuid1)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.set_uuid2 != devices[i + 1]->superblock.set_uuid2)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.set_uuid3 != devices[i + 1]->superblock.set_uuid3)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.size != devices[i + 1]->superblock.size)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.raid_disks != devices[i + 1]->superblock.raid_disks)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.md_minor != devices[i + 1]->superblock.md_minor)
			return(R10_EMISMATCH);
		if(devices[i]->superblock.utime != devices[i + 1]->superblock.utime)
			return(R10_ECLEAN);
		if(devices[i]->superblock.events_lo != devices[i + 1]->superblock.events_lo)
			return(R10_ECLEAN);
		if(devices[i]->superblock.events_hi != devices[i + 1]->superblock.events_hi)
			return(R10_ECLEAN);
		if(devices[i]->superblock.cp_events_lo != devices[i + 1]->superblock.cp_events_lo)
			return(R10_ECLEAN);
		if(devices[i]->superblock.cp_events_hi != devices[i + 1]->superblock.cp_events_hi)
			return(R10_ECLEAN);
	}

	return(R10_SUCCESS);
}

raid1to0_error reordermddevs(mddevice **devices, int devcount) {
	int i;
	int devnum;
	mddevice **newlist;

	newlist = malloc(sizeof(mddevice *) * devcount);
	if(newlist == NULL)
		return(R10_ENOMEM);

	for(i = 0; i < devcount; i++)
		newlist[i] = NULL;
	for(i = 0; i < devcount; i++) {
		devnum = devices[i]->superblock.this_disk.raid_disk;
		if(devnum >= devcount) {
			free(newlist);
			return(R10_EDISK);
		}
		newlist[devnum] = devices[i];
	}
	for(i = 0; i < devcount; i++) {
		if(newlist[i] == NULL) {
			free(newlist);
			return(R10_EDISK);
		}
	}

	devices = newlist;
	return(R10_SUCCESS);
}

raid1to0_error restripe(
		mddevice **devices,
		int devcount,
		unsigned long int chunksize,
		void (*status)(unsigned long int cur, unsigned long int max)) {
	unsigned long int chunks;
	unsigned long int i;
	int curdev = 0;
	unsigned long int outoffset = 0;
	char *buffer;

	buffer = malloc(sizeof(char) * chunksize);
	if(buffer == NULL)
		return(R10_ENOMEM);
	chunks = devices[0]->sbpos / chunksize;

	for(i = 0; i < chunks; i++) {
		if(lseek(devices[0]->fd, i * chunksize, SEEK_SET) < 0)
			return(R10_EGENERIC);
		if(read(devices[0]->fd, buffer, chunksize) < 0)
			return(R10_EGENERIC);
		if(lseek(devices[curdev]->fd, outoffset, SEEK_SET) < 0)
			return(R10_EGENERIC);
		if(write(devices[curdev]->fd, buffer, chunksize) < 0)
			return(R10_EGENERIC);
		curdev++;
		if(curdev == devcount) {
			curdev = 0;
			outoffset += chunksize;
		}
		status(i, chunks);
	}

	if(devices[0]->sbpos > chunks * chunksize) {
		if(lseek(devices[0]->fd, chunks * chunksize, SEEK_SET) < 0)
			return(R10_EGENERIC);
		if(read(devices[0]->fd, buffer, devices[0]->sbpos - (chunks * chunksize)) < 0)
			return(R10_EGENERIC);
		if(write(devices[curdev]->fd, buffer, devices[0]->sbpos - (chunks * chunksize)) < 0)
			return(R10_EGENERIC);
	}
	status(chunks, chunks);

	for(i = 0; i < devcount; i++) {
		devices[i]->superblock.level = 0;
		devices[i]->superblock.size = 0;
		devices[i]->superblock.chunk_size = chunksize;
		devices[i]->superblock.sb_csum = sbcsum(&(devices[i]->superblock));
		if(lseek(devices[i]->fd, devices[i]->sbpos, SEEK_SET) < 0)
			return(R10_EGENERIC);
		if(write(devices[i]->fd, &(devices[i]->superblock), sizeof(mdp_super_t)) < 0)
			return(R10_EGENERIC);
	}

	return(R10_SUCCESS);
}

void raid1to0_perror(raid1to0_error e) {
	switch(e) {
		case R10_SUCCESS:
			fprintf(stderr, "Success");
			break;
		case R10_EGENERIC:
			fprintf(stderr, "A generic error, when no other error fits");
			break;
		case R10_ENOMEM:
			fprintf(stderr, "Failed to allocate memory");
			break;
		case R10_EMAGIC:
			fprintf(stderr, "Bad magic");
			break;
		case R10_EVERSION:
			fprintf(stderr, "Unsupported version");
			break;
		case R10_ECHECKSUM:
			fprintf(stderr, "Bad checksum");
			break;
		case R10_ESIZE:
			fprintf(stderr, "Bad size");
			break;
		case R10_ELEVEL:
			fprintf(stderr, "Not RAID level 1");
			break;
		case R10_EFEATURE:
			fprintf(stderr, "Unsupported feature");
			break;
		case R10_EDISK:
			fprintf(stderr, "Disks unaccounted for");
			break;
		case R10_ECLEAN:
			fprintf(stderr, "Array is not clean or consistent");
			break;
		case R10_EDUPLICATE:
			fprintf(stderr, "Duplicate device found on command line");
			break;
		case R10_EMISMATCH:
			fprintf(stderr, "Devices not of the same array or inconsistent");
			break;
		default:
			fprintf(stderr, "Unknown error code");
	}
}

void closemddev(mddevice *device) {
	close(device->fd);
	free(device->path);
	free(device);
}
