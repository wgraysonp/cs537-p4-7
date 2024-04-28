#include <stdio.h>
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "wfs.h"

char *disk_image = NULL;
struct wfs_sb *super_block;

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
  .readdir = wfs_readdir,
};


void init_disk(char *f_name){
	int fd = open(f_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
	struct stat buf;
	fstat(fd, &buf);
	int disk_size = buf.st_size;

	char *mem = mmap(NULL, disk_size, PROT_WRITE | PROT_READ, fd, 0);
	if (mem == (void *)-1)
		perror("mmap");
	close(fd);
	disk_image = mem;
	super_block = (struct wfs_sb *)mem;
	
}

int main (int argc, char *argv[]){

	init_disk(argv[0]);
	argv++;
	argc--;

	return fuse_main(argc, argv, &ops, NULL);
}
