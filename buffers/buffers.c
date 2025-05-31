#include "buffers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#include "../semaphores/semaphores.h"

#define DEFAULT_BUFFER_SIZE 1024

#define DEBUG_BUFFER_SLEEP 0

void freeStringBufferMemory(StringBuffer* stringBuffer);

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
    sleep(DEBUG_BUFFER_SLEEP);

    if (buf->count >= buf->capacity) { // TODO: Ver si aca dumpeamos al file en lugar de redimensionar
        buf->capacity *= 2;
        buf->data = realloc(buf->data, sizeof(char*) * buf->capacity);
    }

    buf->data[buf->count] = strdup(str);  // copia del string
    buf->count++;

    pthread_mutex_unlock(buf->mutex);
}

void freeBuffer(StringBuffer* stringBuffer, const char* dumpFilePath) {
    // open file for writing
    FILE* file = fopen(dumpFilePath, "w");
    if (file == NULL) {
        perror("Error al abrir el archivo para escribir");
        freeStringBufferMemory(stringBuffer);

        return;
    }

    // Escribir cada string en el archivo
    if (file != NULL) {
        for (int i = 0; i < stringBuffer->count; i++) {
            fprintf(file, "%s\n", stringBuffer->data[i]); // escribir string en el archivo
        }

        fclose(file);
    }

    freeStringBufferMemory(stringBuffer);
}

void freeStringBufferMemory(StringBuffer* stringBuffer) {
    for (int i = 0; i < stringBuffer->count; i++) {
        free(stringBuffer->data[i]);  // free each string
    }

    free(stringBuffer->data);         // free pointers array
    pthread_mutex_destroy(stringBuffer->mutex); // destroy semaphore
    free(stringBuffer->mutex); // free semaphore
    free(stringBuffer);
}