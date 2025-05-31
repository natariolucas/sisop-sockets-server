#ifndef BUFFERS_H
#define BUFFERS_H
#include <pthread.h>


typedef struct {
    char** data;     // array de strings
    int count;       // cuántos hay usados
    int capacity;    // capacidad total (inicial: DEFAULT_BUFFER_SIZE)
    pthread_mutex_t* mutex; // semaphore id
} StringBuffer;

StringBuffer* newBufferWithMutex(pthread_mutex_t* mutex);
void appendToBuffer(StringBuffer* buf, const char* str);
void freeBuffer(StringBuffer* stringBuffer, const char* dumpFilePath);

#endif //BUFFERS_H
