#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "wfs.h"

char *disk_img;
int num_inodes;
int num_blocks;

void parse_args(int argc, char** argv){
	int op;
	while((op = getopt(argc, argv, "d:i:b:")) != -1){
		switch(op){
			case 'd':
			disk_img = (char*)malloc((strlen(optarg)+1)*sizeof(char));
			strcpy(disk_img, optarg);
			break;

			case 'i':
			num_inodes = atoi(optarg);
			break;

			case 'b':
			num_blocks = atoi(optarg);
			int rem = 32 - num_blocks % 32;
			if (rem != 32)
				num_blocks += rem;
		}
	}
}

void init_fs(){
	int inode_bitmap_size = num_inodes/8;
	int block_bitmap_size = num_blocks/8;
	int super_size = sizeof(struct wfs_sb);
	int fd = open(disk_img, O_RDWR);
	if (fd < 0){
		perror("open");
		exit(errno);
	}
	struct wfs_sb super = (struct wfs_sb)malloc(super_size);
	super.num_inodes = num_inodes;
	super.num_data_blocks = num_data_blocks;
	super.i_bitmap_ptr = super_size;
	super.d_bitmap_ptr = super_size + inode_bitmap_size;
	super.i_blocks_ptr = super.d_bitmap_ptr + block_bitmap_size;
	super.d_blocks_ptr = super.i_blocks_ptr + num_inodes*sizeof(struct wfs_inode);
	
	struct stat buf;
	fstat(fd, &buf);
	if (super.d_blocks_ptr + num_blocks*BLOCK_SIZE > bf.st_size)
		//disk image not big enough
		exit(1);

	if( write(fd, super, super_size) == -1){
		perror("write");
		exit(errno);
	}

	//free(super);

	struct wfs_inode root = (struct wfs_inode)malloc(sizeof(struct wfs_inode));
	root.num = 0;
	root.mode = S_IFDIR;
	root.uid = 0;
	root.gid = 0;
	root.size =0;
	root.nlinks = 0;
	root.atim = 0;
	root.mtim = 0;
	root.ctim =0;

	
	uint i = 0b10000000;
	lseek(fd, super.i_bitmap_ptr, SEEK_SET);
       	if (write(fd, i, 1) == -1){
		perror("write");
		exit(errno);
	}

	lseek(fd, super.i_blocks_ptr, SEEK_SET);
	if (write(fd, root, sizeof(struct wfs_inode)) == -1){
		perror("write");
		exit(errno);
	}
	free(super);
	free(root);
	close(fd);	
	
}

int main(int argc, char* argv[]){

	parse_args(argc, argv);
	init_fs();

	return 0;
}

