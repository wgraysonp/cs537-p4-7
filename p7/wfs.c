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
#include <dirent.h>

#define UNUSED -1

char *disk_image = NULL;
struct wfs_sb *super_block;


/*
 * looks for an available inode and returns the availble inode
 * number on success or -1 on failure
 */
int alloc_inode(){
	printf("alloc_inode\n");
	unsigned char check_bit;

	unsigned char *ptr = (unsigned char *)(disk_image + super_block->i_bitmap_ptr);
	//printf("start of i_bitmap block%p\n",(void*) ptr);
	for (int j = 0; j < super_block->num_inodes/8; j++){
		for (int i =0; i < 8; i++){
			check_bit = 1 << i;
			//printf("byte %d contents : 0x%x\n",j, ptr[j]);
			//printf("byte %d contents: 0x%x\n", j+1, ptr[j+1]);
			if (!(check_bit & ptr[j])){
			//	printf("returning inode num: %d\n", j*8+i);
			//	printf("alloc_inode edit address %p\n", (void*)&ptr[j]);
			//	printf("alloc_inode next address %p\n", (void*)&ptr[j+1]);
				ptr[j] |= check_bit;
				return j*8 + i;
			}
		}
	}

	return -1; 

}

/*
 * same as the above function be looks for an empty data block
 */

int alloc_dblock(){
	printf("alloc_dblock\n");
	unsigned char check_bit;

	unsigned char *ptr = (unsigned char*)(disk_image + super_block->d_bitmap_ptr);
	//printf("Start of dbitmap block %p\n", (void*)ptr);
	for (int j = 0; j < super_block->num_data_blocks/8; j++){
		for (int i = 0; i < 8; i++){
			check_bit = 1 << i;
			if (!(check_bit & ptr[j])){
	//			printf("alloc_dblock edit address %p\n", (void*)&ptr[j]);
				ptr[j] |= check_bit;
				return j*8 + i;
			}
		}
	}

	return -1;
}


/*
 * Given a file path, finds the corresponding inode
 * and points inode to it.
 * return 0 on success or -1 on failure
 * TODO: FIX this. its buggy and segfaults
 */
int get_inode(const char *path, struct wfs_inode **inode){

	printf("get_inode\n");

	char delim = '/';
	int inode_num = 0;
	int found = 0;
	*inode = (struct wfs_inode*)(disk_image + super_block->i_blocks_ptr);
	char *path_copy = (char*)malloc((strlen(path)+1)*sizeof(char));
	strcpy(path_copy, path);
	char *token = strtok(path_copy, &delim);
	unsigned char bitcheck = 1;
	unsigned char *valid_bit_loc = NULL;

	(*inode)->atim = time(NULL);
	
	if (token == NULL){
		printf("return rood inode\n");
		free(path_copy);
		return 0;
	}

	if (strcmp(token, "")==0 || strcmp(token, ".") == 0){
		token = strtok(NULL, &delim);
	}

	printf("token %s\n", token);

	while(token != NULL){
		if ((*inode)->mode &  S_IFDIR){ 
			found = 0;
			for (int i = 0; i < N_BLOCKS; i++){
				if ((*inode)->blocks[i] != UNUSED && (*inode)->blocks[i] < super_block->num_data_blocks){
			//		printf("d_blocks_ptr: %ld\n", super_block->d_blocks_ptr);
			//		printf("block: %ld\n", (*inode)->blocks[i]*BLOCK_SIZE);
					struct wfs_dentry *dentry = (struct wfs_dentry*)(disk_image + super_block->d_blocks_ptr + (*inode)->blocks[i]*BLOCK_SIZE);
					for (int j = 0; j < BLOCK_SIZE/sizeof(struct wfs_dentry); j++){
	//					printf("dir name: %s", dentry[j].name);
						if (strcmp(dentry[j].name, token) == 0){
							inode_num = dentry[j].num;
							found = 1;
							break;
						}
					}
				if (found == 1)
					break;
				}
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

		bitcheck = 1 << (inode_num % 8);
		valid_bit_loc = (unsigned char*)(disk_image + inode_num/8 + super_block->i_bitmap_ptr);
		if (!(bitcheck & *valid_bit_loc)){
			printf("Bit check failed in get inode\n");
			printf("valid bits: %x\n", *valid_bit_loc);
			return -1;
		} 

		*inode = (struct wfs_inode*)(disk_image + super_block->i_blocks_ptr + inode_num*BLOCK_SIZE);
		(*inode)->atim = time(NULL);
		token = strtok(NULL, &delim);

	}

	free(path_copy);

	return 0;

}

static int wfs_getattr(const char *path, struct stat *stbuf){

	printf("CALL: getattr\n");
	printf("getattr path arg: %s\n", path);

	struct wfs_inode *inode = NULL;
	
	if (get_inode(path, &inode) == -1){
		return -ENOENT;
	}

	inode->atim = time(NULL);

	stbuf->st_uid = inode->uid;
	stbuf->st_gid = inode->gid;
	stbuf->st_atime = inode->atim;
	stbuf->st_mtime = inode->mtim;
	stbuf->st_mode = inode->mode;
	stbuf->st_size = inode->size;

	// maybe required?
	stbuf->st_ino = inode->num;
	stbuf->st_nlink = inode->nlinks;
	stbuf->st_blksize = BLOCK_SIZE;
	stbuf->st_blocks = inode->size / BLOCK_SIZE;

	return 0;

}


static int wfs_mknod(const char* path, mode_t mode, dev_t dev){

	printf("CALL: mknod\n");
	printf("mode: %x\n", mode);

	char* path_copy = malloc((strlen(path)+1)*sizeof(char));
	strcpy(path_copy, path);

	struct wfs_inode *inode = NULL;
	if (get_inode(path_copy, &inode) != -1){
		free(path_copy);
		printf("ERROR: inode already exists.");
		return -EEXIST;
	}

	int inode_num;
        if ((inode_num = alloc_inode()) == -1){
                free(path_copy);
                printf("ERROR: Failed to allocated inode\n");
                return -ENOSPC;
        }

	inode = (struct wfs_inode*)(disk_image + super_block->i_blocks_ptr + inode_num*BLOCK_SIZE);
	inode->num = inode_num;
	inode->mode = mode;
	inode->uid = getuid();
	inode->gid = getgid();
	inode->size = 0;
	inode->nlinks = 0;
	inode->atim = time(NULL);
	inode->mtim = time(NULL);
	inode->ctim = time(NULL);
	for (int i = 0; i < N_BLOCKS; i++){
		inode->blocks[i] = UNUSED;
	}

	char *f_name = &path_copy[strlen(path)-1];

        while(*f_name != '/'){
                f_name--;
        }
        *f_name = '\0';
        f_name++;


        struct wfs_inode *parent = NULL;
        if (get_inode(path_copy, &parent) == -1){
		free(path_copy);
		printf("ERROR: Failed to get parent inode\n");
		return -ENOENT;
	}

	struct wfs_dentry *dentry;

	if (parent->size >= 512*7){
		free(path_copy);
		printf("ERROR: Parent directory full\n");
		return -ENOSPC;
	} else if (parent->size % 512 == 0){
		int dblock_num;
		printf("creating entry %s with new block\n", f_name);
		if ((dblock_num = alloc_dblock()) == -1){
			free(path_copy);
			printf("ERROR: faiiled to allocate data block\n");
			return -ENOSPC;
		}
		printf("dblock num: %d\n", dblock_num);
		printf("block array entry %ld\n", parent->size/512);
		parent->blocks[parent->size/512] = dblock_num;
		dentry = (struct wfs_dentry*)(disk_image + super_block->d_blocks_ptr + dblock_num*BLOCK_SIZE);
		strcpy(dentry->name, f_name);
		dentry->num = inode_num;
		parent->size += 32;
	} else {
		printf("creating entry %s with used block\n", f_name);
		off_t block_offset = (parent->size % BLOCK_SIZE)/32;
		printf("block_offset %ld\n", block_offset);
		int insert_block = parent->size / BLOCK_SIZE;
		printf("insert block %d\n", insert_block);
		struct wfs_dentry* dentry  = (struct wfs_dentry*)(disk_image + parent->blocks[insert_block]*BLOCK_SIZE + super_block->d_blocks_ptr);
		strcpy(dentry[block_offset].name, f_name);
		dentry[block_offset].num = inode_num;
		parent->size += 32;
	}

	free(path_copy);
	
	return 0;
}

static int wfs_mkdir(const char *path, mode_t mode){

	printf("CALL: mkdir\n");
	
	return wfs_mknod(path, S_IFDIR, 0);	

}

// helper
int parse_path(const char *path, char **parent_path, char **filename) {
        const char *ptr = path + strlen(path) - 1; // begin at end of path
        const char *parent = NULL;
        int parent_size = 0;
        while (ptr >= path) {
                if (*ptr == '/') {
                        parent = ptr;
                        break;
                }
                ptr--;
        }
        if (parent == NULL) {
        	return -1;
        } else {
                parent_size = parent - path;
                *parent_path = strndup(path, parent_size);
                *filename = strdup(parent + 1);
        }
	return 0;
} 


static int wfs_unlink(const char *path) {
	printf("CALL: unlink\n");
        struct wfs_inode *inode; // = malloc(sizeof(struct wfs_inode));
        struct wfs_inode *parent_inode; // = malloc(sizeof(struct wfs_inode));
        char *parent_path;
        char *filename;

        if (get_inode(path, &inode) == -1) {
                return -ENOENT;
        }

        if (parse_path(path, &parent_path, &filename) == -1) {
		return -ENOENT;
	}
        if (get_inode(parent_path, &parent_inode) == -1) {
                return -ENOENT;
        }

	// free inode
	int inode_byte = inode->num / 8; // bitmap byte containing inode
	int inode_bit = inode->num % 8; // bitmap bit containing inode
	unsigned char bit_mask = ~(1 << inode_bit);
	unsigned char *inode_bitmap = (unsigned char*)(disk_image + super_block->i_bitmap_ptr);

	inode_bitmap[inode_byte] &= bit_mask; // set inode to 0

	// free data blocks
	for (int i = 0; i < IND_BLOCK; i++) {
		if (inode->blocks[i] != UNUSED) {
			int block_byte = inode->blocks[i] / 8; // byte containing block
			int block_bit = inode->blocks[i] % 8; // bit containing block
			printf("block byte: %d\n", block_byte);
			printf("block_bit: %d\n", block_bit);
			unsigned char block_bit_mask = ~(1 << block_bit);
			unsigned char *block_bitmap = (unsigned char*)(disk_image + super_block->d_bitmap_ptr);

			block_bitmap[block_byte] &= block_bit_mask; // set block to 0
		}
	}

	off_t *ind_block = (off_t*)(disk_image + super_block->d_blocks_ptr + inode->blocks[IND_BLOCK]*BLOCK_SIZE);
	if(inode->blocks[IND_BLOCK] != UNUSED){
	for (int i = 0; i < BLOCK_SIZE/sizeof(off_t); i++){
		if(!(inode->mode & S_IFDIR)){
			off_t block_byte = ind_block[i]/8;
		//	printf("block_byte: %ld\n", block_byte);
			off_t block_bit = ind_block[i]%8;
		//	printf("block_bit: %ld\n", block_bit);
			unsigned char block_bit_mask = ~(1 << block_bit);
			unsigned char *block_bitmap = (unsigned char*)(disk_image + super_block->d_bitmap_ptr);
			if (block_byte != 0){
				printf("block_byte in if: %ld\n", block_byte);
				block_bitmap[block_byte] &= block_bit_mask;
			}
		}
	}
	}

	// remove inode directory entry
	for (int i = 0; i < D_BLOCK; i++) {
		if (parent_inode->blocks[i] != 0) {
			struct wfs_dentry *dentry = (struct wfs_dentry*)(disk_image + super_block->d_blocks_ptr + parent_inode->blocks[i] * BLOCK_SIZE);
			for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
				if (strcmp(dentry[j].name, filename) == 0) {
					dentry[j].num = 0; // set directory entry to 0
				}
			}
		}

	}
	//free(inode);

	parent_inode->mtim = time(NULL);
	parent_inode->ctim = time(NULL);

        return 0;
}

static int wfs_rmdir(const char *path){
	printf("CALL: rmdir\n");
	struct wfs_inode *inode; // = malloc(sizeof(struct wfs_inode));
	struct wfs_inode *parent_inode;
	char *parent_path;
	char *dirname;

	if (get_inode(path, &inode) == -1) {
                return -ENOENT;
        }
	if (parse_path(path, &parent_path, &dirname) == -1) {
		return -ENOENT;
	}

	if (get_inode(parent_path, &parent_inode) == -1) {
		return -ENOENT;
	}

	 // free inode
        int inode_byte = inode->num / 8; // bitmap byte containing inode
        int inode_bit = inode->num % 8; // bitmap bit containing inode
	unsigned char bit_mask = ~(1 << inode_bit);
        unsigned char *inode_bitmap = (unsigned char*)(disk_image + super_block->i_bitmap_ptr);

        inode_bitmap[inode_byte] &= bit_mask; // set inode to 0
					      //
	// free bitmap entries for directories
	//
	for (int i = 0; i < IND_BLOCK; i++){
		if (inode->blocks[i] != UNUSED) {
                        int block_byte = inode->blocks[i] / 8; // byte containing block
                        int block_bit = inode->blocks[i] % 8; // bit containing block
                        unsigned char block_bit_mask = ~(1 << block_bit);
                        unsigned char *block_bitmap = (unsigned char*)(disk_image + super_block->d_bitmap_ptr);

                        block_bitmap[block_byte] &= block_bit_mask; // set block to 0
                }

	}
	

	// remove parent directory entry
        for (int i = 0; i < D_BLOCK; i++) {
        	if (parent_inode->blocks[i] != 0) {
                        struct wfs_dentry *dentry = (struct wfs_dentry*)(disk_image + super_block->d_blocks_ptr + parent_inode->blocks[i] * BLOCK_SIZE);
                        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                                if (strcmp(dentry[j].name, dirname) == 0) {
                                        dentry[j].num = 0; // set directory entry to 0
                                }
                        }
                }

        }

	parent_inode->mtim = time(NULL);
        parent_inode->ctim = time(NULL);
	parent_inode->size -= 32;

	return 0;
}


static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
        printf("CALL: read\n");
        struct wfs_inode *inode;
        if (get_inode(path, &inode) == -1) {
                return -ENOENT;
        }

        size_t read = 0;
        while (read < size) {
        size_t to_read = size - read;
        off_t block_index = (offset + read) / BLOCK_SIZE;
        off_t block_offset = (offset + read) % BLOCK_SIZE;
        size_t bytes_block = BLOCK_SIZE - block_offset; // bytes remaining in block
        size_t bytes_to_read;

        if (bytes_block < to_read) {
                bytes_to_read = bytes_block;
        } else {
                bytes_to_read = to_read;
        }


        char *data;
        if (block_index < D_BLOCK) {
                data = disk_image + inode->blocks[block_index] * BLOCK_SIZE + block_offset;
                memcpy(buf + read, data, bytes_to_read);
        } else {
                size_t *ind_block = (size_t *)(disk_image + super_block->d_blocks_ptr + inode->blocks[IND_BLOCK]);
                data = disk_image + ind_block[block_index] * BLOCK_SIZE + block_offset;
                memcpy(buf, data, bytes_to_read);
        }
        // read
        read += bytes_to_read;
 }
 //inode->atim = time(NULL);
        return read;
}




static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	printf("CALL: write\n");

	size_t bytes_written = 0;
	size_t to_write;

	struct wfs_inode *inode;
	if (get_inode(path, &inode)==-1){
		return -ENOENT;
	}

	off_t block_index = offset / BLOCK_SIZE;
	off_t block_offset = offset % BLOCK_SIZE;
	off_t indirect_offset;
	off_t indirect_ptr;

	void *dest = NULL;
	int blocks_alloced = 0;

	printf("write size: %ld\n", size);

	while(bytes_written < size && block_index < IND_BLOCK){
		printf("Bytes written to direct blocks: %ld\n", bytes_written);
		dest = (void*)(disk_image + super_block->d_blocks_ptr + inode->blocks[block_index]*BLOCK_SIZE + block_offset);
		if (block_offset == 0 && block_index == IND_BLOCK - 1){
			break;
		} else if (block_offset == 0){
			int d_block_num;
			if ((d_block_num = alloc_dblock()) == -1){
				return ENOSPC;
			}
			printf("alloc d block in direct blocks\n");
			blocks_alloced++;
			printf("blocks_allocated: %d\n", blocks_alloced);
			block_index++;
			inode->blocks[block_index] = d_block_num;
			dest = (void*)(disk_image + super_block->d_blocks_ptr + inode->blocks[block_index]*BLOCK_SIZE + block_offset);
		}

		to_write = size - bytes_written;
		if (to_write > BLOCK_SIZE - block_offset)
			to_write = BLOCK_SIZE - block_offset;
		memcpy(dest, (void*)buf, to_write);
		inode->size += to_write;
		bytes_written += to_write;
		buf += to_write;
		block_offset = 0;
	}

	// indirect block;
	//
	// printf(
	
	indirect_offset = offset - 7*BLOCK_SIZE;
        if(indirect_offset < 0)
           	indirect_offset = 0;

	indirect_ptr = indirect_offset/BLOCK_SIZE*sizeof(off_t);

	size_t *ind_block;

	printf("bytes written after direct blocks %ld\n", bytes_written);

	while(bytes_written < size){
		if (inode->blocks[IND_BLOCK] == UNUSED){
			printf("allocate indirect block\n");
			int d_block_num;
			if ((d_block_num = alloc_dblock()) == -1)
				return ENOSPC;
			inode->blocks[IND_BLOCK] = d_block_num;
		}
		ind_block = (size_t*)(disk_image + super_block->d_blocks_ptr + inode->blocks[IND_BLOCK]*BLOCK_SIZE);
		if (indirect_offset > BLOCK_SIZE*BLOCK_SIZE/sizeof(off_t)){
			return ENOSPC;
		}
		if(indirect_offset % BLOCK_SIZE == 0){
			int d_block_num;
			if((d_block_num = alloc_dblock()) == -1){
				return ENOSPC;
			}
			printf("allocate block and place pointer in indirect block\n");
			
			ind_block[indirect_offset/BLOCK_SIZE] = d_block_num;
			dest = (void*)(disk_image + super_block->d_blocks_ptr + d_block_num*BLOCK_SIZE + block_offset);
		} else {

			dest = (void*)(disk_image + super_block->d_blocks_ptr + ind_block[indirect_ptr]*BLOCK_SIZE + block_offset);

		}

		to_write = size - bytes_written;
                if (to_write > BLOCK_SIZE - block_offset)
                        to_write = BLOCK_SIZE - block_offset;
                memcpy(dest, (void*)buf, to_write);
		inode->size += to_write;
                bytes_written += to_write;
                buf += to_write;
                block_offset = 0;


	}


	return bytes_written;
}


// ls is working
static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
	printf("CALL: readdir\n");
	struct wfs_inode *inode;

	if (get_inode(path, &inode) == -1) {
		return -ENOENT;
	}

	int read = 0;
	for (int i = 0; i < D_BLOCK; i++) {
		if (inode->blocks[i] != UNUSED && inode->blocks[i] < super_block->num_data_blocks) {
			struct wfs_dentry *dentry = (struct wfs_dentry*)(disk_image + super_block->d_blocks_ptr + inode->blocks[i] * BLOCK_SIZE);
			for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
				// begin reading at offset
                                if (read >= offset)  {
					if (dentry[j].num != 0) {
						printf("Dir name in readdir: %s\n", dentry[j].name);
						printf("Dir num: %d\n", dentry[j].num);
						printf("Block num: %d\n", i);
						printf("Inode num: %d\n", inode->num);
						if (filler(buf, dentry[j].name, NULL, 0) != 0) {
							return 0; // completed
						}
					}
				read++;
				}
			}

		}
	}

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
  .mkdir = wfs_mkdir,
  .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
  .readdir = wfs_readdir,
};

int main (int argc, char *argv[]){
	init_disk(argv[1]);
	argv = &argv[2];
	argc-=2;
	return fuse_main(argc, argv, &ops, NULL);
}
