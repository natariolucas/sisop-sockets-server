#include "semaphores.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>

pthread_mutex_t* newMutex() {
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);

    return mutex;
}

pthread_cond_t* newCond() {
    pthread_cond_t* cond = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(cond, NULL);

    return cond;
}

int createSemaphore(int initialValue) {
    int semID = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);  // create set of 1 semaphore
    if (semID == -1) {
        perror("[!] semget");
        exit(1);
    }

    union semun semaphoreArguments;
    semaphoreArguments.val = initialValue;
    if (semctl(semID, 0, SETVAL, semaphoreArguments) == -1) {
        perror("semctl");
        exit(1);
    }

    return semID;
}

void freeSemaphore(int semID) {
    semctl(semID, 0, IPC_RMID);
}

void PSemaphore(int semaphoreID) {
    singleOperationSemaphore(semaphoreID, -1);
}

void VSemaphore(int semaphoreID) {
    singleOperationSemaphore(semaphoreID, 1);
}

int GetValSemaphore(int semaphoreID) {
    return semctl(semaphoreID, 0, GETVAL);
}

void singleOperationSemaphore(int semaphoreID, short operation) {
    /*
    * 0 is the index of semaphore set
    * -1 means decrementing by 1
    * 0 refers to flags
    */
    struct sembuf operationParams = {0, operation, 0};

    // The 1 in third param means that there is only 1 operation (single operation)
    int semResult = semop(semaphoreID, &operationParams, 1);  // Blocks if current semaphore value is 0
    if (semResult == -1) {
        perror("[!] semaphore operation");
        exit(1);
    }
}

void destroySemaphore(int semID) {
    if (semctl(semID, 0, IPC_RMID) == -1) {
        perror("[!] Error al eliminar el semáforo");
        exit(1);
    }
}

void destroyMutex(pthread_mutex_t* mutex) {
    pthread_mutex_destroy(mutex);
}

void destroyCond(pthread_cond_t* cond) {
    pthread_cond_destroy(cond);
}