#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/mman.h>
#include <malloc.h>

#include "circular_buffer.h"
#include "shm.h"

// Private functions

static void advance_pointer_n(cbuf_handle_t cbuf, size_t len)
{
    assert(cbuf);

    if(cbuf->internal->full)
    {
        cbuf->internal->tail = (cbuf->internal->tail + len) % cbuf->internal->max;
    }

    cbuf->internal->head = (cbuf->internal->head + len) % cbuf->internal->max;

    // We mark full because we will advance tail on the next time around
    cbuf->internal->full = (cbuf->internal->head == cbuf->internal->tail);
}

static void advance_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    if(cbuf->internal->full)
    {
        cbuf->internal->tail = (cbuf->internal->tail + 1) % cbuf->internal->max;
    }

    cbuf->internal->head = (cbuf->internal->head + 1) % cbuf->internal->max;

    // We mark full because we will advance tail on the next time around
    cbuf->internal->full = (cbuf->internal->head == cbuf->internal->tail);
}

static void retreat_pointer_n(cbuf_handle_t cbuf, size_t len)
{
    assert(cbuf && cbuf->internal);

    cbuf->internal->full = false;
    cbuf->internal->tail = (cbuf->internal->tail + len) % cbuf->internal->max;
}

static void retreat_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    cbuf->internal->full = false;
    cbuf->internal->tail = (cbuf->internal->tail + 1) % cbuf->internal->max;
}

// User APIs

cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
{
    assert(buffer && size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    cbuf->internal = memalign(SHMLBA, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    cbuf->buffer = buffer;
    cbuf->internal->max = size;
    circular_buf_reset(cbuf);
    atomic_flag_clear(&cbuf->internal->acquire);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

cbuf_handle_t circular_buf_init_shm(size_t size, key_t key)
{
    assert(size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    if (shm_is_created(key, size))
    {
        fprintf(stderr, "shm key %u already created. Re-creating.\n", key);
        shm_destroy(key, size);
    }
    shm_create(key, size);

    cbuf->buffer = shm_attach(key, size);
    assert(cbuf->buffer);

    key++;
    if (shm_is_created(key, sizeof(struct circular_buf_t_aux)))
    {
        fprintf(stderr, "shm key %u already created. Re-creating.\n", key);
        shm_destroy(key, sizeof(struct circular_buf_t_aux));
    }
    shm_create(key, sizeof(struct circular_buf_t_aux));

    cbuf->internal = shm_attach(key, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    cbuf->internal->max = size;
    atomic_flag_clear(&cbuf->internal->acquire);

    circular_buf_reset(cbuf);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

cbuf_handle_t circular_buf_connect_shm(size_t size, key_t key)
{
    assert(size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    cbuf->buffer = shm_attach(key, size);
    assert(cbuf->buffer);

    cbuf->internal = shm_attach(key+1, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    assert (cbuf->internal->max == size);

    return cbuf;
}


void circular_buf_free(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);
    free(cbuf->internal);
    free(cbuf);
}

void circular_buf_free_shm(cbuf_handle_t cbuf, size_t size, key_t key)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);
    shm_dettach(key, size, cbuf->buffer);
    shm_destroy(key, size);
    shm_dettach(key+1, sizeof(struct circular_buf_t_aux), cbuf->internal);
    shm_destroy(key+1, sizeof(struct circular_buf_t_aux));
    free(cbuf);
}

void circular_buf_reset(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    cbuf->internal->head = 0;
    cbuf->internal->tail = 0;
    cbuf->internal->full = false;

    atomic_flag_clear(&cbuf->internal->acquire);
}

size_t circular_buf_size(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    size_t size = circular_buf_capacity(cbuf);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    if(!cbuf->internal->full)
    {
        if(cbuf->internal->head >= cbuf->internal->tail)
        {
            size = (cbuf->internal->head - cbuf->internal->tail);
        }
        else
        {
            size = (cbuf->internal->max + cbuf->internal->head - cbuf->internal->tail);
        }

    }

    atomic_flag_clear(&cbuf->internal->acquire);

    return size;
}

size_t circular_buf_free_size(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    size_t size = 0;

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    if(!cbuf->internal->full)
    {
        if(cbuf->internal->head >= cbuf->internal->tail)
        {
            size = cbuf->internal->max - (cbuf->internal->head - cbuf->internal->tail);
        }
        else
        {
            size = (cbuf->internal->tail - cbuf->internal->head);
        }

    }

    atomic_flag_clear(&cbuf->internal->acquire);

    return size;
}

size_t circular_buf_capacity(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    size_t capacity = cbuf->internal->max;

    atomic_flag_clear(&cbuf->internal->acquire);

    return capacity;
}

void circular_buf_put(cbuf_handle_t cbuf, uint8_t data)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    cbuf->buffer[cbuf->internal->head] = data;
    advance_pointer(cbuf);

    atomic_flag_clear(&cbuf->internal->acquire);
}

int circular_buf_put2(cbuf_handle_t cbuf, uint8_t data)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);

    int r = -1;

    if(!circular_buf_full(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        cbuf->buffer[cbuf->internal->head] = data;
        advance_pointer(cbuf);

        atomic_flag_clear(&cbuf->internal->acquire);
        r = 0;
    }

    return r;
}

int circular_buf_get(cbuf_handle_t cbuf, uint8_t * data)
{
    assert(cbuf && data && cbuf->internal && cbuf->buffer);

    int r = -1;

    if(!circular_buf_empty(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        *data = cbuf->buffer[cbuf->internal->tail];
        retreat_pointer(cbuf);

        atomic_flag_clear(&cbuf->internal->acquire);
        r = 0;
    }
    return r;
}

bool circular_buf_empty(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    bool is_empty = !cbuf->internal->full && (cbuf->internal->head == cbuf->internal->tail);

    atomic_flag_clear(&cbuf->internal->acquire);

    return is_empty;
}

bool circular_buf_full(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    bool is_full = cbuf->internal->full;

    atomic_flag_clear(&cbuf->internal->acquire);

    return is_full;
}


int circular_buf_get_range(cbuf_handle_t cbuf, uint8_t *data, size_t len)
{
    assert(cbuf && data && cbuf->internal && cbuf->buffer);

#if 0
    for (int i = 0; i < len; i++)
        circular_buf_get(cbuf, &data[i]);

    return 0;
#endif

#if 1
    int r = -1;
    size_t size = circular_buf_capacity(cbuf);

    if(circular_buf_size(cbuf) >= len)
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        if ( ((cbuf->internal->tail + len) % size) > cbuf->internal->tail)
        {
            memcpy(data, cbuf->buffer + cbuf->internal->tail, len);
        }else
        {
            memcpy(data, cbuf->buffer + cbuf->internal->tail, size - cbuf->internal->tail);
            memcpy(data + (size - cbuf->internal->tail), cbuf->buffer, len - (size - cbuf->internal->tail));
        }
        retreat_pointer_n(cbuf, len);

        atomic_flag_clear(&cbuf->internal->acquire);

        r = 0;
    }

    return r;
#endif
}

int circular_buf_put_range(cbuf_handle_t cbuf, uint8_t * data, size_t len)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);

#if 0
    for (int i = 0; i < len; i++)
        circular_buf_put(cbuf, data[i]);

    return 0;
#endif

#if 1
    int r = -1;
    size_t size = circular_buf_capacity(cbuf);
    // test if buffer has enough free space

    if(!circular_buf_full(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));
        if ( ((cbuf->internal->head + len) % size) > cbuf->internal->head)
        {
            memcpy(cbuf->buffer + cbuf->internal->head, data, len);
        } else
        {
            memcpy(cbuf->buffer + cbuf->internal->head, data, size - cbuf->internal->head);
            memcpy(cbuf->buffer, data + (size - cbuf->internal->head), len - (size - cbuf->internal->head));
        }

        advance_pointer_n(cbuf, len);
        atomic_flag_clear(&cbuf->internal->acquire);
        r = 0;
    }

    return r;
#endif
}
