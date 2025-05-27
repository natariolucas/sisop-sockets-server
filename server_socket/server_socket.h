#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H
#include <stdbool.h>
#include <arpa/inet.h>

typedef struct {
    int acceptedSocketFD;
    struct sockaddr_in address;
    bool acceptedSuccessfully;
    int error;
} AcceptedSocket;

int createIPv4Socket();
struct sockaddr_in* createIPv4Address(char* ip, int port);
AcceptedSocket * acceptIncomingConnection(int serverSocketFD);
void startAcceptingIncomingConnections(int serverSocketFD, int semaphoreID, void*(*callback)(int,int));
void acceptConnectionsAndProcessRequests(int serverSocketFD);

#endif //SERVER_SOCKET_H
