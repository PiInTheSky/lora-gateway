#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lifo_buffer.h"

void lifo_buffer_init(lifo_buffer_t *buf, uint32_t length)
{    
    pthread_mutex_init(&buf->Mutex, NULL);
    pthread_cond_init(&buf->Signal, NULL);
    
    pthread_mutex_lock(&buf->Mutex);
    buf->Head = 0;
    buf->Tail = 0;
    buf->Length = length;
    buf->Data = malloc(length * sizeof(void *));
    buf->Quit = false;
    pthread_mutex_unlock(&buf->Mutex);
}

/* Lossy when buffer is full */
void lifo_buffer_push(lifo_buffer_t *buf, void *data_ptr)
{
    pthread_mutex_lock(&buf->Mutex);

    /* If no space, remove oldest from bottom of the queue by advancing Tail */
    if(buf->Head==(buf->Tail-1) || (buf->Head==(buf->Length-1) && buf->Tail==0))
    {
        if(buf->Tail==(buf->Length-1))
            buf->Tail=0;
        else
            buf->Tail++;
    }

    if(buf->Head==(buf->Length-1))
        buf->Head=0;
    else
        buf->Head++;

    buf->Data[buf->Head] = data_ptr;
    
    pthread_cond_signal(&buf->Signal);

    pthread_mutex_unlock(&buf->Mutex);
}

/* Returns NULL when unsuccessful */
void *lifo_buffer_pop(lifo_buffer_t *buf)
{
    void *result;
    
    pthread_mutex_lock(&buf->Mutex);
    if(buf->Head!=buf->Tail)
    {
        result = buf->Data[buf->Head];

        if(buf->Head==0)
            buf->Head=(buf->Length-1);
        else
            buf->Head--;
    
        pthread_mutex_unlock(&buf->Mutex);
    }
    else
    {
        pthread_mutex_unlock(&buf->Mutex);
        
        result = NULL;
    }

    return result;
}

void *lifo_buffer_waitpop(lifo_buffer_t *buf)
{    
    void *result;

    pthread_mutex_lock(&buf->Mutex);
    
    while(buf->Head==buf->Tail && !buf->Quit) /* If buffer is empty */
    {
        /* Mutex is atomically unlocked on beginning waiting for signal */
        pthread_cond_wait(&buf->Signal, &buf->Mutex);
        /* and locked again on resumption */
    }

    if(buf->Quit)
    {
        return NULL;
    }
    
    result = buf->Data[buf->Head];

    if(buf->Head==0)
        buf->Head=(buf->Length-1);
    else
        buf->Head--;

    pthread_mutex_unlock(&buf->Mutex);
    
    return result;
}

void lifo_buffer_quitwait(lifo_buffer_t *buf)
{
    pthread_mutex_lock(&buf->Mutex);

    buf->Quit = true;
    
    pthread_cond_signal(&buf->Signal);

    pthread_mutex_unlock(&buf->Mutex);
}