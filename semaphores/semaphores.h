#ifndef SEMAPHORES_H
#define SEMAPHORES_H
#include <pthread.h>

// Definir union semun si no está definida por la implementación
#if defined(_POSIX_C_SOURCE) && !defined(_DARWIN_C_SOURCE)

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;  // opcional, para GETALL/IPC_INFO
};
#endif

int createSemaphore(int initialValue);
void destroySemaphore(int semID);
void PSemaphore(int semaphoreID);
void VSemaphore(int semaphoreID);
int GetValSemaphore(int semaphoreID);
void singleOperationSemaphore(int semaphoreID, short operation);
pthread_mutex_t* newMutex();
void destroyMutex(pthread_mutex_t* mutex);
pthread_cond_t* newCond();
void destroyCond(pthread_cond_t* cond);
void freeSemaphore(int semID);

#endif //SEMAPHORES_H
