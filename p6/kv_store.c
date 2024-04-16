#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"
#include "ring_buffer.h"
#include "kv_store.h"

#define MAX_THREADS 128

#define PUT_STR "put"
#define GET_STR "get"
#define DEL_STR "del"

#define READY 1
#define NOT_READY 0

struct thread_context {
	int tid;
};

hash_t *Htable = NULL;
int init_table_size = 1000;

int num_threads = 4;
pthread_t threads[MAX_THREADS];
struct thread_context contexts[MAX_THREADS];
struct buffer_descriptor *client_request;

struct ring *ring = NULL;
char *shmem_area = NULL;
char shm_file[] = "shmem_file";

int win_size = 1;

void List_init(list_t *L){
	L->head = NULL;
	pthread_mutex_init(&L->lock, NULL);
}

int List_Insert(list_t *L, key_type key, value_type val){
	pthread_mutex_lock(&L->lock);
	node_t *new = malloc(sizeof(node_t));
	if (new == NULL){
		perror("malloc");
		pthread_mutex_unlock(&L->lock);
		return -1;
	}
	new->key = key;
	new->val = val;
	new->next = L->head;
	L->head = new;
	pthread_mutex_unlock(&L->lock);
	return 0;
}

int List_Lookup(list_t *L, key_type key){
	pthread_mutex_lock(&L->lock);
	node_t *curr = L->head;
	while (curr) {
		if (curr->key == key){
			pthread_mutex_unlock(&L->lock);
			return curr->val;
		}
		curr = curr->next;
	}
	pthread_mutex_unlock(&L->lock);
	return -1;
}

void Hash_Init(int s, hash_t **H) {
	
	hash_t *ht;

	ht = (hash_t*)malloc(sizeof(hash_t));
	ht->size = s;
	ht->lists = (list_t*)malloc(s*sizeof(list_t)); 

	for (int i = 0; i < s; i++){
		List_init(&ht->lists[i]);
	}

	*H = ht;
}

int Hash_Insert(hash_t *H, key_type key, value_type val){
	int s = H->size;
	return List_Insert(&H->lists[hash_function(key, s)], key, val);
}

int Hash_Lookup(hash_t *H, key_type key){
	int s = H->size;
	return List_Lookup(&H->lists[hash_function(key, s)], key);
}


int put(key_type k, value_type v){
	return Hash_Insert(Htable, k, v);
}

int get(key_type k){
	return Hash_Lookup(Htable, k);
}

void init_server(){
	int fd = open(shm_file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
	struct stat buf;
	fstat(fd, &buf);
	int shm_size = buf.st_size;


	char *mem = mmap(NULL, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (mem == (void *)-1)
		perror("mmap");

	close(fd);
	shmem_area = mem;
	ring = (struct ring *)mem;
}

void process_reqs(){
	struct buffer_descriptor bd;
	value_type v;

	memset(&bd, 0, sizeof(struct buffer_descriptor));
	ring_get(ring, &bd);
	struct buffer_descriptor *res = (struct buffer_descriptor*)(shmem_area + bd.res_off);
	if (bd.req_type == PUT){
		printf("Server: put key %d val %u\n", bd.k, bd.v);
		put(bd.k, bd.v);
		bd.ready = READY;
	} else {
		if ((v = get(bd.k)) == -1){
			printf("Server: key %d\n not found", bd.k);
			bd.v = 0;
		} else {
			printf("Server: key %d found. Returning value %u\n", bd.k, bd.v);
			bd.v = v;
		}
		bd.ready = READY;
	}
	memcpy(res, &bd, sizeof(struct buffer_descriptor));

}

void *thread_function(void *arg){
	struct thread_context *ctx = arg;
	while(true){
		process_reqs();
		printf("looping\n");
	}
}


void start_threads(){
	printf("STARTING SERVER THREADS\n");
	for (int i = 0; i < num_threads; i++){
		contexts[i].tid = i;
		if (pthread_create(&threads[i], NULL, &thread_function, &contexts[i]))
			perror("pthread_create");
	}

}


void wait_for_threads(){
	for (int i = 0; i < num_threads; i++){
		if (pthread_join(threads[i], NULL)){
			perror("pthread_join");
		}
	}
}

int parse_args(int argc, char**argv){
	int op;
	while((op = getopt(argc, argv, "n:s:")) != -1){
	       switch(op){
	       	case 'n':
		num_threads = atoi(optarg);
		break;

		case 's':
		init_table_size = atoi(optarg);
		}
	}
	return 0;
}
	      	       

int main(int argc, char* argv[]){

	if (parse_args(argc, argv) != 0){
		exit(1);
	}

	Hash_Init(num_threads, &Htable);

	init_server();
	start_threads();
	wait_for_threads();

	return 0;
}
