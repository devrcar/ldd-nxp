#ifndef RING_BUFF_HELPER_H
#define RING_BUFF_HELPER_H

#include "nxp_simtemp.h"

/*
** Function Prototypes
*/
void rb_put(simtemp_ring_buff_t *rb, simtemp_sample_t *value);

int rb_get(simtemp_ring_buff_t *rb, simtemp_sample_t *value);

int rb_peek(simtemp_ring_buff_t *rb, simtemp_sample_t *value);

bool rb_is_empty(simtemp_ring_buff_t *rb);

#endif