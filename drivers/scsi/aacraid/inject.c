#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#define BUF	255

#define CTL_CODE(function, method) (                 \
    (4<< 16) | ((function) << 2) | (method) \
)
#define METHOD_BUFFERED                 0
#define METHOD_NEITHER                  3
#define FSACTL_ERROR_INJECT		CTL_CODE(9000, METHOD_BUFFERED)
struct aac_error_inject_str {
	unsigned char	type;
	unsigned char 	value;
};


int PrintHelp (int noHeader) 
{
	if (noHeader == 0) {
		printf("\ninject revision 1.00		(c) 2014 PMC-Sierra, Inc.");

		printf("\n-------------------------------------------------------");
		printf("\nError injection tool");
	}
	printf("\n\nUsage");
	printf("\ninject <type> <value>");
	printf("\n<type>:  SCSI_ERROR | TGT_FAILURE");
	printf("\n<value>: type==SCSI_ERROR:");
	printf("\n\tCHECK_CONDITION | BUSY | RESERVATION_CONFLICT |");
	printf("\n\tTASK_SET_FULL | TASK_ABORTED");
	printf("\n<value>: type==TGT_FAILURE:");
	printf("\n\tHBAMODE_DISABLED | IO_ERROR | IO_ABORTED | NO_PATH_TO_DEVICE |");
	printf("\n\tNO_PATH_TO_DEVICE | INVALID_DEVICE | DEVICE_FAULT | UNDERRUN");
	printf("\n");
	return (0);
}

int GetMajor ()
{
	int major = -1;
	FILE *f;
	char buffer[BUF];

	f = fopen("/proc/devices", "r");
	if (f == NULL)
		return(major);

	while (fgets(buffer, BUF, f) != NULL) {
		if (strstr(buffer, "aac") != NULL) {
			major = atoi(buffer);
			break;
		}
	}

	fclose(f);
	return(major);
}
				
int main( int argc, char *argv[] )
{
	int i, major;
	int hDevice = -1;
	char dPath[] = "/dev/aac0";
	char buffer[BUF];
	struct aac_error_inject_str inj;

	if (argc != 3) {
        	PrintHelp(0);
        	return(0);
 	}

	if (!strcmp(argv[1], "SCSI_ERROR")) {
		inj.type = 1;
		if (!strcmp(argv[2], 		"CHECK_CONDITION"))
			inj.value = 0;
		else if (!strcmp(argv[2], 	"BUSY"))
			inj.value = 1;
		else if (!strcmp(argv[2], 	"RESERVATION_CONFLICT"))
			inj.value = 2;
		else if (!strcmp(argv[2], 	"TASK_SET_FULL"))
			inj.value = 3;
		else if (!strcmp(argv[2], 	"TASK_ABORTED"))
			inj.value = 4;
		else {
			PrintHelp(1);
			return(-1);
		}
	} else if (!strcmp(argv[1], "TGT_FAILURE")) {
		inj.type = 2;
		if (!strcmp(argv[2], 		"HBAMODE_DISABLED"))
			inj.value = 0;
		else if (!strcmp(argv[2], 	"IO_ERROR"))
			inj.value = 1;
		else if (!strcmp(argv[2], 	"IO_ABORTED"))
			inj.value = 2;
		else if (!strcmp(argv[2], 	"NO_PATH_TO_DEVICE"))
			inj.value = 3;
		else if (!strcmp(argv[2], 	"INVALID_DEVICE"))
			inj.value = 4;
		else if (!strcmp(argv[2], 	"DEVICE_FAULT"))
			inj.value = 5;
		else if (!strcmp(argv[2], 	"UNDERRUN"))
			inj.value = 6;
		else {
			PrintHelp(1);
			return(-1);
		}
	} else {
		PrintHelp(1);
		return(-1);
	}

	hDevice = open(dPath, O_RDWR);
	if (hDevice == -1) {
		if (errno != ENOENT) {
			printf("\nOpen aac0 device failed, errno=%d\n", errno);
			return(-1);
		}
		if ((major = GetMajor()) == -1) {
			printf("\nOpen failed, driver not loaded?\n");
			return(-1);
		}
		sprintf(buffer, "mknod %s c %d 0", dPath, major);
		system(buffer);	
		hDevice = open(dPath, O_RDWR);
		if (hDevice == -1) {
			printf("\nOpen aac0 device failed, errno=%d\n", errno);
			return(-1);
		}
	}

	printf("Inject type %d value %d\n", inj.type, inj.value);
	if (ioctl(hDevice, FSACTL_ERROR_INJECT, &inj) == -1) {
		printf("\nIOCTL failed, errno=%d\n", errno);
	}

	close(dPath);
	return(0);
} 
