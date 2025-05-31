#ifndef SEMAPHORES_H
#define SEMAPHORES_H
#include <stdbool.h>
// #include <sys/_pthread/_pthread_cond_t.h>
// #include <sys/_pthread/_pthread_mutex_t.h>
#include <pthread.h>

// Definir union semun si no está definida por la implementación
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;  // opcional, para GETALL/IPC_INFO
};


int createSemaphore(int initialValue);
void PSemaphore(int semaphoreID);
void VSemaphore(int semaphoreID);
int GetValSemaphore(int semaphoreID);
void singleOperationSemaphore(int semaphoreID, short operation);
pthread_mutex_t* newMutex();
pthread_cond_t* newCond();
void freeSemaphore(int semID);

#endif //SEMAPHORES_H
