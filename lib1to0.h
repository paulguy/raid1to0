#include <linux/fs.h>
#include <asm/types.h>
#include "md_p.h"

typedef struct {
	char *path;
	int fd;
	unsigned long int size;
	unsigned long int sbpos;
	mdp_super_t superblock;
} mddevice;

typedef enum {
	R10_SUCCESS,	/* success */
	R10_EGENERIC,	/* A generic error where nothing else fits */
	R10_ENOMEM,		/* Memory allocation failed */
	R10_EMAGIC,		/* Bad magic */
	R10_EVERSION,	/* Unsupported version */
	R10_ECHECKSUM,	/* bad checksum */
	R10_ESIZE,		/* Size makes no sense */
	R10_ELEVEL,		/* Not RAID level 1 */
	R10_EFEATURE,	/* Unsupported feature */
	R10_EDISK,		/* Not all disks accounted for */
	R10_ECLEAN,		/* array isn't clean */
	R10_EDUPLICATE,	/* duplicate device found */
	R10_EMISMATCH	/* Devices not from same array or inconsistent */
} raid1to0_error;

inline unsigned long int sbposition(unsigned long int size);
unsigned int checksum(unsigned int *data, unsigned int length);
unsigned int sbcsum(mdp_super_t *sb);
mddevice *openmddev(const char *pathname);
void dumpmddev(mddevice *device);
raid1to0_error verifymddevs(mddevice **devices, int devcount);
raid1to0_error reordermddevs(mddevice **devices, int devcount);
raid1to0_error restripe(
		mddevice **devices,
		int devcount,
		unsigned long int chunksize,
		void (*status)(unsigned long int cur, unsigned long int max));
void closemddev(mddevice *device);
void raid1to0_perror(raid1to0_error e);
