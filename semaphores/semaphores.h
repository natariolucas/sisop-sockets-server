#ifndef SEMAPHORES_H
#define SEMAPHORES_H
#include <stdbool.h>
#include <sys/_pthread/_pthread_mutex_t.h>

int createSemaphore(int initialValue);
void PSemaphore(int semaphoreID);
void VSemaphore(int semaphoreID);
int GetValSemaphore(int semaphoreID);
void singleOperationSemaphore(int semaphoreID, short operation);
pthread_mutex_t* newMutex();
void freeSemaphore(int semID);

#endif //SEMAPHORES_H
