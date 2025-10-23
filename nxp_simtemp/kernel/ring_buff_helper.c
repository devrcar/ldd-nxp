#include "ring_buff_helper.h"

void rb_put(simtemp_ring_buff_t *rb, simtemp_sample_t *value)
{
	rb->readings[rb->head] = *value;
	rb->head = (rb->head + 1) % TEMP_SAMPLE_BUF_SIZE;

	/* Move read ptr if buffer is full */
	if (rb->head == rb->tail) {
		rb->tail = (rb->tail + 1) % TEMP_SAMPLE_BUF_SIZE;
	}
}

int rb_get(simtemp_ring_buff_t *rb, simtemp_sample_t *value)
{
    if (rb_is_empty(rb)) {
        return 0;
    }

    *value = rb->readings[rb->tail];
    /* Updating read ptr */
	rb->tail = (rb->tail + 1) % TEMP_SAMPLE_BUF_SIZE;
	return 1;
}

int rb_peek(simtemp_ring_buff_t *rb, simtemp_sample_t *value)
{
    if (rb_is_empty(rb)) {
        return 0;
    }

    *value = rb->readings[rb->tail];
	return 1;
}

inline bool rb_is_empty(simtemp_ring_buff_t *rb)
{
	return (rb->tail == rb->head);
}
