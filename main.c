#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "semaphores/semaphores.h"
#include "server_socket/server_socket.h"
#include "regex.h"
#include "buffers/buffers.h"
#include "operations/operations.h"

#define LISTEN_BACKLOG 1
#define MAX_CONNECTIONS 5
#define PORT 2000
#define IP ""

// Colors
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KBLU  "\x1B[34m"
#define KCYN  "\x1B[36m"

//resultFilePaths
#define SUM_FILE_PATH "./sum_result.txt"
#define SUB_FILE_PATH "./sub_result.txt"
#define TIMES_FILE_PATH "./times_result.txt"
#define DIV_FILE_PATH "./div_result.txt"

void* processSocketThreadRequests(int acceptedSocketFD, int connectionSemaphore);
void createBuffers();
void releaseBuffers();
StringBuffer* getOperationBuffer(const char operator);
void writeBuffer(char* operation, char* result, StringBuffer* buffer);
void removeAllCRLF(char* str);

StringBuffer* sumBuffer;
StringBuffer* subBuffer;
StringBuffer* timesBuffer;
StringBuffer* divBuffer;

int main() {
    printf("%s[+] starting with IP: '%s' and PORT: %d\n", KCYN, IP, PORT);

    const int serverSocketFD = createIPv4Socket();
    if (serverSocketFD == -1) {
        perror("[!] error while opening socket\n");
        return -1;
    }

    struct sockaddr_in* serverAddress = createIPv4Address(IP, PORT);
    if ( serverAddress == NULL) {
        perror("[!] error while creating IPv4 Address\n");
        free(serverAddress);

        return -1;
    };

    int bindResult = bind(serverSocketFD, (struct sockaddr*)serverAddress, sizeof(*serverAddress)); // Attach socket (process) to port
    if (bindResult == -1) {
        perror("[!] error while binding socket\n");
        free(serverAddress);

        return 0;
    }

    free(serverAddress);
    printf("%s[+] socket was bound successfully\n", KCYN);

    int listenResult = listen(serverSocketFD, LISTEN_BACKLOG);
    if (listenResult == -1) {
        perror("[!] error while listening incoming connections\n");

        return 0;
    }

    createBuffers();

    pthread_mutex_t* establishedConnectionsMutex = newMutex();
    pthread_cond_t* noConnectionsCond = newCond();

    StartAcceptingConnectionsParams params;
    params.availableConnectionsSemaphore = createSemaphore(MAX_CONNECTIONS);
    params.serverSocketFD = serverSocketFD;
    params.callback = processSocketThreadRequests;

    EstablishedConnections establishedConnections;
    params.establishedConnections = &establishedConnections;
    params.establishedConnections->firstClientConnected = false;
    params.establishedConnections->count = 0;
    params.establishedConnections->mutex = establishedConnectionsMutex;
    params.establishedConnections->noConnectionsCond = noConnectionsCond;


    pthread_t threadAcceptConnectionsID;
    pthread_create(&threadAcceptConnectionsID, NULL, startAcceptingIncomingConnections, &params);

    pthread_mutex_lock(params.establishedConnections->mutex);
    while (!params.establishedConnections->firstClientConnected || (params.establishedConnections->firstClientConnected && params.establishedConnections->count > 0)) {
        pthread_cond_wait(params.establishedConnections->noConnectionsCond, params.establishedConnections->mutex);
    }

    pthread_mutex_unlock(params.establishedConnections->mutex);

    printf("%s[!] shutting down....",KRED);

    releaseBuffers();
    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void* processSocketThreadRequests(int acceptedSocketFD, int connectionSemaphore) {
    char request[1024];
    char response[1024];
    regex_t regex;

    int regexCompilationResult = compileRegexOperations(&regex);
    if (regexCompilationResult != 0 ) {
        perror("[!] error while compiling regex\n");

        close(acceptedSocketFD);
        VSemaphore(connectionSemaphore);
        regfree(&regex);

        return NULL;
    }

    while (true) {
        ssize_t amountReceived = recv(acceptedSocketFD, request, sizeof(request), 0);
        if (amountReceived < 0) {
            char errorMessage[1024];

            sprintf(errorMessage, "[-] fd: %d - error while receiving data\n", acceptedSocketFD);
            perror(errorMessage);

            break;
        }

        if (amountReceived == 0) {
            printf("%s[-] fd: %d - socket closed\n", KRED, acceptedSocketFD);
            break;
        }

        request[amountReceived] = '\0';

        int regexResult;
        char operator;

        double result = getOperationResultFromRequest(&regex, request, &regexResult, &operator);
        switch (regexResult) {
            case 0: {
                char resultStr[100];
                sprintf(resultStr, "%.2f", result);
                removeAllCRLF(request);

                printf("%s[*] resolved expression %s with result: %s\n", KCYN, request, resultStr);

                StringBuffer* operationBuffer = getOperationBuffer(operator);
                if (operationBuffer == NULL) {
                    strcpy(response, "error al escribir en buffer, intente nuevamente");
                    break;
                }

                printf("%s[*] writing to buffer: %p\n", KBLU, operationBuffer);
                writeBuffer(request, resultStr, operationBuffer);
                printf("%s[*] wrote to buffer: %p\n", KBLU, operationBuffer);

                sprintf(response, "RESULT = %s", resultStr);

                break;
            }
            case REG_NOMATCH:
                strcpy(response, "expresión inválida, verifique la sintaxis de su expresión");
                break;
            default:
                strcpy(response, "error al validar su expresion, intente nuevamente");
        }

        send(acceptedSocketFD, response, sizeof (response), 0);
    }

    regfree(&regex);

    return NULL;
}

void createBuffers() {
    sumBuffer = newBufferWithMutex(newMutex());
    printf("%s[*] created sum buffer: %p\n", KBLU, sumBuffer);

    subBuffer = newBufferWithMutex(newMutex());
    printf("%s[*] created sub buffer: %p\n", KBLU, subBuffer);

    timesBuffer = newBufferWithMutex(newMutex());
    printf("%s[*] created times buffer: %p\n", KBLU, timesBuffer);

    divBuffer = newBufferWithMutex(newMutex());
    printf("%s[*] created div buffer: %p\n", KBLU, divBuffer);

}

void releaseBuffers() {
    StringBuffer* resultsBuffers[4] = {sumBuffer, subBuffer, timesBuffer, divBuffer};
    char* resultsPaths[4] = {SUM_FILE_PATH, SUB_FILE_PATH, TIMES_FILE_PATH, DIV_FILE_PATH};

    for (int i = 0; i < 4; i++) {
        pthread_mutex_lock(resultsBuffers[i]->mutex); // it won't be unlocked because it is destroyed
        freeBuffer(resultsBuffers[i], resultsPaths[i]);
    }
}

void writeBuffer(char* operation, char* result, StringBuffer* buffer) {
    char* log = malloc(sizeof(char)*1024);
    sprintf(log, "%s = %s \n", operation, result);

    appendToBuffer(buffer, log);
}

StringBuffer* getOperationBuffer(const char operator) {
    StringBuffer* buffer;
    switch (operator) {
        case '+':
            buffer = sumBuffer;
            break;
        case '-':
            buffer = subBuffer;
            break;
        case '*':
            buffer = timesBuffer;
            break;
        case '/':
            buffer = divBuffer;
            break;
        default:
            return NULL;
    }

    return buffer;
}

void removeAllCRLF(char* str) {
    int i = 0, j = 0;
    while (str[i]) {
        if (str[i] != '\r' && str[i] != '\n') {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}