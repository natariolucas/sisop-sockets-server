#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H
#include <stdbool.h>
#include <arpa/inet.h>

typedef struct {
    bool firstClientConnected;
    int count;
    pthread_mutex_t* mutex;
    pthread_cond_t* noConnectionsCond;
} EstablishedConnections;

typedef struct {
    int serverSocketFD;
    int availableConnectionsSemaphore;
    EstablishedConnections* establishedConnections;
    void*(*callback)(int,int);
} StartAcceptingConnectionsParams;

typedef struct {
    int acceptedSocketFD;
    struct sockaddr_in address;
    bool acceptedSuccessfully;
    int error;
} AcceptedSocket;

int createIPv4Socket();
struct sockaddr_in* createIPv4Address(char* ip, int port);
AcceptedSocket * acceptIncomingConnection(int serverSocketFD);
void* startAcceptingIncomingConnections(void* param);
void acceptConnectionsAndProcessRequests(int serverSocketFD);

#endif //SERVER_SOCKET_H
