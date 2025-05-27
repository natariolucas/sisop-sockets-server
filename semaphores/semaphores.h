#ifndef SEMAPHORES_H
#define SEMAPHORES_H
#include <stdbool.h>

int createSemaphore(int initialValue);
void PSemaphore(int semaphoreID);
void VSemaphore(int semaphoreID);
void singleOperationSemaphore(int semaphoreID, short operation);
int newMutex(bool startsInOne);
void freeSemaphore(int semID);

#endif //SEMAPHORES_H
