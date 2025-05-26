#ifndef SEMAPHORES_H
#define SEMAPHORES_H

int createSemaphore(int initialValue);
void PSemaphore(int semaphoreID);
void VSemaphore(int semaphoreID);
void singleOperationSemaphore(int semaphoreID, short operation);

#endif //SEMAPHORES_H
