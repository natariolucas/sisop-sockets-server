#include "buffers.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread/pthread.h>

#include "../semaphores/semaphores.h"

#define DEFAULT_BUFFER_SIZE 1024

StringBuffer* newBufferWithMutex(pthread_mutex_t* mutex) {
    StringBuffer* buf = malloc(sizeof(StringBuffer));
    buf->data = malloc(sizeof(char*) * DEFAULT_BUFFER_SIZE);
    buf->count = 0;
    buf->capacity = DEFAULT_BUFFER_SIZE;
    buf->mutex = mutex;
    return buf;
}

void appendToBuffer(StringBuffer* buf, const char* str) {
    pthread_mutex_lock(buf->mutex);
    sleep(2);

    if (buf->count >= buf->capacity) { // TODO: Ver si aca dumpeamos al file en lugar de redimensionar
        buf->capacity *= 2;
        buf->data = realloc(buf->data, sizeof(char*) * buf->capacity);
    }

    buf->data[buf->count] = strdup(str);  // copia del string
    buf->count++;

    pthread_mutex_unlock(buf->mutex);
}

void freeBuffer(StringBuffer* buf) {
    for (int i = 0; i < buf->count; i++) {
        free(buf->data[i]);  // liberar cada string
    }
    free(buf->data);         // liberar array de punteros
    pthread_mutex_destroy(buf->mutex); // destroy semaforo
    free(buf->mutex); // libera semaforo
    free(buf);               // liberar struct
}
