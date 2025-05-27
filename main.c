#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "semaphores.h"
#include "server_socket.h"
#include "regex.h"
#include "operations.h"

#define LISTEN_BACKLOG 128
#define MAX_CONNECTIONS 5
#define PORT 2000
#define IP ""

// Colors
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KBLU  "\x1B[34m"
#define KCYN  "\x1B[36m"

void* processSocketRequests(int acceptedSocketFD, int connectionSemaphore);
void removeAllCRLF(char* str);

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

    int semaphoreConnectionsID = createSemaphore(MAX_CONNECTIONS);

    startAcceptingIncomingConnections(serverSocketFD, semaphoreConnectionsID, processSocketRequests);

    printf("%s[!] shutting down....",KRED);
    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void* processSocketRequests(int acceptedSocketFD, int connectionSemaphore) {
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

        double result = getOperationResultFromRequest(&regex, request, &regexResult);
        switch (regexResult) {
            case 0: {
                if (regexResult != 0) {
                    strcpy(response, "error al procesar la operación, verifique su expresión");
                    break;
                }

                sprintf(response, "RESULT = %.2f", result);
                removeAllCRLF(request);
                printf("%s[*] resolving expression %s with response: %s\n", KCYN, request, response);

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

    close(acceptedSocketFD);
    VSemaphore(connectionSemaphore);
    regfree(&regex);

    return NULL;
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