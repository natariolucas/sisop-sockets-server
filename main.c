#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <asm-generic/errno.h>
#include <sys/stat.h>

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
#define BASE_RESULTS_DIR "./results/"
#define SUM_FILE_PATH "./results/sums.txt"
#define SUB_FILE_PATH "./results/subs.txt"
#define TIMES_FILE_PATH "./results/times.txt"
#define DIV_FILE_PATH "./results/divs.txt"

void handleSIGINT(int signal);
void handleShutdown(const int serverSocketFD, int availableConnectionsSemaphore,
                    pthread_t threadAcceptConnectionsID, pthread_mutex_t *establishedConnectionsMutex, pthread_cond_t *noConnectionsCond);

void* processSocketThreadRequests(int acceptedSocketFD, int connectionSemaphore);
void createBuffers();
void releaseBuffers();
StringBuffer* getOperationBuffer(const char operator);
void writeBuffer(char* operation, char* result, StringBuffer* buffer);
void removeAllCRLF(char* str);
void createDirectories(const char *path);

volatile sig_atomic_t allowRunning = 1;

StringBuffer* sumBuffer;
StringBuffer* subBuffer;
StringBuffer* timesBuffer;
StringBuffer* divBuffer;

int main() {
    signal(SIGINT, handleSIGINT);

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
    while (allowRunning &&
        (
            !params.establishedConnections->firstClientConnected ||
            (params.establishedConnections->firstClientConnected && params.establishedConnections->count > 0)
        )) {

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);  // Obtener tiempo actual
        ts.tv_sec += 1;  // Incrementar el tiempo por 1 segundo

        // Usar pthread_cond_timedwait con el timeout de 1 segundo
        int rc = pthread_cond_timedwait(params.establishedConnections->noConnectionsCond,
                                         params.establishedConnections->mutex, &ts);

        if (rc == ETIMEDOUT) {
            // Si se alcanza el tiempo máximo sin que llegue una señal, podemos hacer algo aquí
            printf("Timeout: no se alcanzó la condición de cierre de server en 1 segundo,  verificando cierre brusco.\n");
        }
    }
    pthread_mutex_unlock(params.establishedConnections->mutex);

    handleShutdown(serverSocketFD, params.availableConnectionsSemaphore, threadAcceptConnectionsID, params.establishedConnections->mutex, params.establishedConnections->noConnectionsCond);

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

    while (allowRunning) {
        ssize_t amountReceived = recv(acceptedSocketFD, request, sizeof(request), 0);
        if (amountReceived < 0) {
            printf("%s[!] aborted receiving in socket %d, socket closed\n",KRED, acceptedSocketFD);

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

void handleShutdown(const int serverSocketFD, int availableConnectionsSemaphore,
                    pthread_t threadAcceptConnectionsID, pthread_mutex_t *establishedConnectionsMutex, pthread_cond_t *noConnectionsCond) {
    releaseBuffers();

    printf("%s[!] closing socket\n", KRED);
    shutdown(serverSocketFD, SHUT_RDWR);
    close(serverSocketFD);

    printf("%s[!] waiting accept connections thread to finish...\n", KRED);
    pthread_join(threadAcceptConnectionsID, NULL);
    printf("%s[!] accept connections thread finished\n", KGRN);

    printf("%s[!] destroying available connections semaphore\n",KRED);
    destroySemaphore(availableConnectionsSemaphore);

    printf("%s[!] destroying established connections mutex\n",KRED);
    destroyMutex(establishedConnectionsMutex);

    printf("%s[!] destroying no connections condition\n",KRED);
    destroyCond(noConnectionsCond);

    printf("%s[!] shutting down....\n",KRED);
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
    createDirectories(BASE_RESULTS_DIR);

    StringBuffer* resultsBuffers[4] = {sumBuffer, subBuffer, timesBuffer, divBuffer};
    char* resultsPaths[4] = {SUM_FILE_PATH, SUB_FILE_PATH, TIMES_FILE_PATH, DIV_FILE_PATH};

    for (int i = 0; i < 4; i++) {
        printf("%s[!] destroying buffer %p\n",KRED, resultsBuffers[i]);
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

void createDirectories(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755); // Ignora si ya existe
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

void handleSIGINT(int signal) {
    printf("%s[!!!] Unexpected end of process - Handling SIGINT \n", KRED);
    allowRunning = 0;
}