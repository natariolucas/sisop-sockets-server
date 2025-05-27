#include "semaphores.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/_types/_key_t.h>

int newMutex(bool startsInOne) {
    if (startsInOne) {
        return createSemaphore(1);
    }

    return createSemaphore(0);
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