#ifndef __LIFO_BUFFER_H__
#define __LIFO_BUFFER_H__

#include <stdbool.h>
#include <pthread.h>

typedef struct
{
    /* Buffer Access Lock */
    pthread_mutex_t Mutex;
    /* New Data Signal */
    pthread_cond_t Signal;
    /* Whether the waiting thread should quit */
    bool Quit;
    /* Head and Tail Indexes */
    uint32_t Head, Tail;
    /* Data */
    void **Data;
    /* Data Length */
    uint32_t Length;
} lifo_buffer_t;

/** Common functions **/
void lifo_buffer_init(lifo_buffer_t *buf, uint32_t length);
uint32_t lifo_buffer_queued(lifo_buffer_t *buf);
void lifo_buffer_push(lifo_buffer_t *buf, void *data_ptr);
void *lifo_buffer_pop(lifo_buffer_t *buf);
void *lifo_buffer_waitpop(lifo_buffer_t *buf);
void lifo_buffer_quitwait(lifo_buffer_t *buf);
bool lifo_buffer_requeue(lifo_buffer_t *buf, void *data_ptr);

#endif /* __LIFO_BUFFER_H__*/