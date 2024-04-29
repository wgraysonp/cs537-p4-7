#include <stdio.h>
#include "wfs.h"
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

char *disk_image = NULL;
struct wfs_sb *super_block;


/*
 * looks for an available inode and returns the availble inode
 * number on success or -1 on failure
 */
int alloc_inode(){
	int curr_bits;
	int check_bit;
	int inode_num = 0;

	char *ptr = (char *)super_block->i_bitmap_ptr;
	while ((off_t)ptr < super_block->d_bitmap_ptr){
		curr_bits = *ptr;
		check_bit = 1;
		for (int i =0; i < 8; i++){
			check_bit <<= 1;
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
 * same as the above function be looks for an empty data block
 */

int alloc_dblock(){
	uint32_t curr_bits;
	uint32_t check_bit;
	int dblock_num = 0;

	char *ptr = (char*)super_block->d_bitmap_ptr;
	while((off_t)ptr < super_block->i_blocks_ptr){
		curr_bits = *ptr;
		check_bit = 1;
		for (int i = 0; i < 8; i++){
			check_bit <<= i;
			if (!(check_bit && curr_bits)){
				return dblock_num;
			}
			dblock_num++;
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
	inode = (struct wfs_inode*)super_block->i_blocks_ptr;
	char *path_copy = (char*)malloc((strlen(path)+1)*sizeof(char));
	strcpy(path_copy, path);
	char *token = strtok(path_copy, &delim);
	uint32_t bitcheck = 1;
	char* valid_bit_loc = NULL;

	inode->atim = time(NULL);

	// move past the root directory
	if (strcmp(token, "")==0 || strcmp(token, ".") == 0){
		token = strtok(NULL, &delim);
	}

	while(token != NULL){
		if (inode->mode == S_IFDIR){ 
			found = 0;
			for (int i = 0; i < IND_BLOCK; i++){
				for (int j = 0; j < 512; j+= 32){
					off_t data_loc = (off_t)disk_image + inode->blocks[i] + j;
					dir = (struct wfs_dentry*)data_loc;
					if (strcmp(dir->name, token) == 0){
						inode_num = dir->num;
						found = 1;
						break;
					}
				}
				if (found == 1)
					break;
			}

			if (found == 0){
				free(path_copy);
				// made it through the loop without finding the entry
				return -1;
			}
		} else {
			free(path_copy);
			// made it to a file before the end of the given path
			// so the path is invalid
			return -1;
		}
		inode = (struct wfs_inode*)(super_block->i_blocks_ptr + inode_num*sizeof(struct wfs_inode));
		bitcheck = bitcheck << (inode_num % 8);
		valid_bit_loc = (char*)(inode_num/8 + super_block->i_bitmap_ptr);
		if (!(bitcheck && (uint32_t)*valid_bit_loc)){
			return -1;
		}
		inode->atim = time(NULL);
		token = strtok(NULL, &delim);

	}

	free(path_copy);

	return 0;

}

static int wfs_getattr(const char *path, struct stat *stbuf){

	struct wfs_inode *inode = NULL;
	
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


static int wfs_mknod(const char* path, mode_t mode, dev_t dev){

	char* path_copy = malloc((strlen(path)+1)*sizeof(char));
	strcpy(path_copy, path);

	int inode_num;
	if ((inode_num = alloc_inode()) == -1){
		free(path_copy);
		return ENOSPC;
	}

	struct wfs_inode *inode = NULL;
	if (get_inode(path_copy, inode) != -1){
		free(path_copy);
		return EEXIST;
	}

	inode = (struct wfs_inode*)(super_block->i_blocks_ptr + inode_num*sizeof(struct wfs_inode));
	inode->num = inode_num;
	inode->mode = mode;
	inode->uid = getuid();
	inode->gid = getgid();
	inode->size = 0;
	inode->nlinks = 0;
	inode->atim = time(NULL);
	inode->mtim = time(NULL);
	inode->ctim = time(NULL);

	int dblock_num;
	if ((dblock_num = alloc_dblock()) == -1){
		free(path_copy);
		return ENOSPC;
	} 

	char *f_name = &path_copy[strlen(path)-1];

        while(*f_name != '\''){
                f_name--;
        }
        *f_name = '\0';
        f_name++;


        struct wfs_inode *parent = NULL;
        if (get_inode(path_copy, parent) == -1){
		free(path_copy);
		return ENOENT;
	}

	struct wfs_dentry *dentry;

	if (parent->size >= 512*7){
		free(path_copy);
		return ENOSPC;
	} else if (parent->size % 512 == 0){
		int dblock_num;
		if ((dblock_num = alloc_dblock()) == -1){
			free(path_copy);
			return ENOSPC;
		}
		parent->blocks[parent->size/512] = dblock_num;
		dentry = (struct wfs_dentry*)(super_block->d_blocks_ptr + dblock_num*BLOCK_SIZE);
		strcpy(dentry->name, f_name);
		dentry->num = inode_num;
	} else {
		off_t block_offset = parent->size % BLOCK_SIZE;
		int insert_block = parent->size / BLOCK_SIZE;
		off_t block_start = parent->blocks[insert_block]*BLOCK_SIZE + super_block->d_blocks_ptr;
		dentry = (struct wfs_dentry*)(block_start + block_offset);
		strcpy(dentry->name, f_name);
		dentry->num = inode_num;
	}

	free(path_copy);
	
	return 0;
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

// add functions to this struct as they are completed. 
// the code wont compile if its in the struct but not defined
// elsewhere
static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
};

int main (int argc, char *argv[]){

	init_disk(argv[0]);
	argv++;
	argc--;

	return fuse_main(argc, argv, &ops, NULL);
}
