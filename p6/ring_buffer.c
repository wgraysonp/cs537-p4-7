#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "ring_buffer.h"
#include "kv_store.h"
#include "common.h"


/*
 * Initialize the ring
 * @param r A pointer to the ring
 * @return 0 on success, negative otherwise - this negative value will be
 * printed to output by the client program
*/
int init_ring(struct ring *r) {
	r->p_head = 0;
	r->p_tail = 0;
	r->c_head = 0;
	r->c_tail = 0;
	if (pthread_mutex_init(&r->mutex, NULL) != 0) {
		return -1;
	}
}

/*
 * Submit a new item - should be thread-safe
 * This call will block the calling thread if there's not enough space
 * @param r The shared ring
 * @param bd A pointer to a valid buffer_descriptor - This pointer is only
 * guaranteed to be valid during the invocation of the function
*/
void ring_submit(struct ring *r, struct buffer_descriptor *bd) {
	pthread_mutex_lock(&r->mutex);
	uint32_t next = (r->p_head + 1) % 1024;
	while (next == r->c_tail) {
		pthread_mutex_unlock(&r->mutex);
		// reacquire mutex and check for space
		pthread_mutex_lock(&r->mutex);
		next = (r->p_head + 1) % 1024;
	}

	r->buffer[r->p_head] = *bd;
	r->p_head = next;
	r->p_tail = r->p_head;
	pthread_mutex_unlock(&r->mutex);
}

/*
 * Get an item from the ring - should be thread-safe
 * This call will block the calling thread if the ring is empty
 * @param r A pointer to the shared ring
 * @param bd pointer to a valid buffer_descriptor to copy the data to
 * Note: This function is not used in the clinet program, so you can change
 * the signature.
*/
void ring_get(struct ring *r, struct buffer_descriptor *bd) {
	pthread_mutex_lock(&r->mutex);
	uint32_t next = (r->c_head + 1) % 1024;
	while (r->c_head == r->p_tail) {
		pthread_mutex_unlock(&r->mutex);
		// reacquire mutex and check for space
                pthread_mutex_lock(&r->mutex);
	}
	*bd = r->buffer[r->c_head]; 
	r->c_head = next;
	r->c_tail = r->c_head;
	pthread_mutex_unlock(&r->mutex);

	/*
	if (bd->req_type == PUT) {
		int putval = put(bd->k, bd->v);
	}
	if (bd->req_type == GET) {
		int getval = get(bd->k);
	} */
}
