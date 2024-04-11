#include <pthread.h>
#include "common.h"

typedef struct __node_t {
	key_type key;
	value_type val;
	struct __node_t *next;
} node_t;

typedef struct __list_t {
	node_t *head;
	pthread_mutex_t lock;
} list_t;

typedef struct __hash_t {
	int size;
	list_t *lists;
} hash_t;
