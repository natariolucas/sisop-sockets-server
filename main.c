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
char* getStringFromMatchIndex(regmatch_t* matches, int index, char* source);
double* processOperationFromString(const char* operand1, char operator, const char* operand2);

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

    const char* regexRule = "^([+-]?[0-9]+)[ ]*([+*/-])[ ]*([+-]?[0-9]+)[\r\n]*$";
    regex_t regex;

    int regexCompilationResult = regcomp(&regex, regexRule, REG_EXTENDED);
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
        regmatch_t matches[4];

        int regexResult = regexec(&regex, request, 4, matches, 0);
        switch (regexResult) {
            case 0: {
                char *operand1 = getStringFromMatchIndex(matches, 1, request);
                char *operator = getStringFromMatchIndex(matches, 2, request);
                char *operand2 = getStringFromMatchIndex(matches, 3, request);

                double *result = processOperationFromString(operand1, *operator, operand2);
                if (result == NULL) {
                    strcpy(response, "error al procesar la operación, verifique su expresión");

                    free(operand1);
                    free(operator);
                    free(operand2);
                    free(result);
                    break;
                }

                sprintf(response, "RESULT = %.2f", *result);
                printf("%s[*] resolving expression %s%s%s with response: %s\n", KCYN, operand1,operator,operand2, response);

                free(operand1);
                free(operator);
                free(operand2);
                free(result);
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

char* getStringFromMatchIndex(regmatch_t* matches, int index, char* source) {
    long long startOffset = matches[index].rm_so;
    int charCount = (int)(matches[index].rm_eo - startOffset);
    char* result = malloc(sizeof(char) * (charCount + 1)); // +1 for end of string
    strncpy(result, source + startOffset, charCount);
    result[charCount] = '\0';

    return result;
}

double* processOperationFromString(const char* operand1, const char operator, const char* operand2) {
    char* endPointerOfParseOperand1;
    const double op1 = strtod(operand1, &endPointerOfParseOperand1);

    char* endPointerOfParseOperand2;
    const double op2 = strtod(operand2, &endPointerOfParseOperand2);

    double* result = malloc(sizeof(double));

    switch (operator) {
        case '+':
            *result = op1 + op2;
            break;
        case '-':
            *result = op1 - op2;
            break;
        case '*':
            *result = op1 * op2;
            break;
        case '/':
            *result = op1 / op2;
            break;
        default:
            return NULL;
    }

    return result;
}
