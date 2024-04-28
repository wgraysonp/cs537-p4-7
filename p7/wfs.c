#include <stdio.h>
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
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


/*
 * looks for an available inode and returns the availble inode
 * number on success or -1 on failure
 */
int alloc_inode(){
	uint32_t curr_bits;
	uint32_t check_bit;
	int inode_num = 0;

	char *ptr = super_block->i_bitmap_ptr;
	while (ptr < super_block->d_bitmap_ptr){
		curr_bits = *ptr;
		check_bit = 1;
		for (int i =0; i < 8; i++){
			check_bit << i;
			if (!(check_bit && curr_bits)){
				return inode_num;
			}
			inode_num++;
		}
		ptr++;
	}

	return -1;

}


/*
 * Given a file path, finds the corresponding inode
 * and points inode to it.
 * return 0 on success or -1 on failure
 */
int get_inode(const char *path, struct wfs_inode *inode){
	char delim = '/';
	int inode_num = 0;
	int found = 0;
	struct wfs_dentry *dir;
	inode = super_block;
	char *token = strtock(path, &delim);
	uint32_t bitcheck = 1;
	int valid_bit_loc = 0;

	inode->atim = time(NULL);


	if (strcmp(token, "")==0){
		token = strtock(NULL, &delim);
	}

	while(token != NULL){
		if (inode->mode == S_IFDIR){ 
			found = 0;
			for (int i = 0; i < N_BLOCKS; i++){
				dir = (struct wfs_dentry*)disk_image[inode->blocks[i]];
				if (strcmp(dir->name, token) == 0){
					inode_num = dir->num;
					found = 1;
					break;
				}
			}

			if (found == 0){
				// made it through the loop without finding the entry
				return -1
			}
		} else {
			// made it to a file before the end of the given path
			// so the path is invalid
			return -1;
		}

		inode = (struct wfs_inode)(super_block->i_blocks_ptr + inode_num*sizeof(struct wfs_inode));
		bitcheck = bitcheck << (inode_num % 8);
		valid_bit_loc = inode_num/8 + super_block->i_bitmap_ptr;
		if (!(bitcheck && (uint32_t)*valid_bit_loc)){
			return -1;
		}
		inode->atim = time(NULL);
		token = strtock(NULL, &delim);

	}

	return 0;

}

static int wfs_getattr(const char *path, struct stat *stbuf){

	struct wfs_inode *inode;
	
	if (get_inode(path, inode) == -1){
		return ENOENT;
	}

	inode->atim = time(NULL);

	stbuf->st_uid = inode->uid;
	stbuf->st_gid = inode->gid;
	stbuf->st_atime = inode->atim;
	stbuf->st_mtime = inode->mtim;
	stbuf->st_mode = inode->mode;
	stbuf->st_size = inode->size;

	return 0;

}

static int wfs_mknod(const char *path, mode_t mode){

	int inode_num;
	if ((inode_num = alloc_inode()) == -1){
		return EEXIST;
	}
}


void init_disk(char *f_name){
	int fd = open(f_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
	struct stat buf;
	fstat(fd, &buf);
	int disk_size = buf.st_size;

	char *mem = mmap(NULL, disk_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
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
