#ifndef BUFFERS_H
#define BUFFERS_H

typedef struct {
    char** data;     // array de strings
    int count;       // cuántos hay usados
    int capacity;    // capacidad total (inicial: DEFAULT_BUFFER_SIZE)
    int mutex; // semaphore id
} StringBuffer;

StringBuffer* newBufferWithMutex(int mutex);
void appendToBuffer(StringBuffer* buf, const char* str);
void freeBuffer(StringBuffer* buf);

#endif //BUFFERS_H
