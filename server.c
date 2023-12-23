// Server Program

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUFLEN 512
#define PORT 27015
#define ADDRESS "127.0.0.1"
#define MAX_CLIENTS 5

typedef struct
{
    SOCKET socket;
    char username[20];
} ClientInfo;

int main()
{
    printf("=== Chat Server ===\n");

    int res, sendRes;

    // INITIALIZATION ===========================
    WSADATA wsaData;
    res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res)
    {
        printf("Startup failed: %d\n", res);
        return 1;
    }
    // ==========================================

    // SETUP SERVER =============================

    SOCKET listener;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
    {
        printf("Error with construction: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    char multiple = !0;
    res = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &multiple, sizeof(multiple));
    if (res < 0)
    {
        printf("Multiple client setup failed: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ADDRESS);
    address.sin_port = htons(PORT);
    res = bind(listener, (struct sockaddr *)&address, sizeof(address));
    if (res == SOCKET_ERROR)
    {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    res = listen(listener, SOMAXCONN);
    if (res == SOCKET_ERROR)
    {
        printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    // ==========================================

    printf("Accepting on %s:%d\n", ADDRESS, PORT);

    // MAIN LOOP ================================

    fd_set socketSet;
    ClientInfo clients[MAX_CLIENTS];
    int curNoClients = 0;
    SOCKET sd, max_sd;
    struct sockaddr_in clientAddr;
    int clientAddrlen;
    char running = !0;

    char recvbuf[BUFLEN];

    char *welcome = "\nWelcome to the chat, please enter your username:";
    int welcomeLength = strlen(welcome);
    char *full = "Sorry, the server is full :(\n";
    int fullLength = strlen(full);
    char *goodbye = "Goodbye.\n";
    int goodbyeLength = strlen(goodbye);

    memset(clients, 0, MAX_CLIENTS * sizeof(ClientInfo));

    while (running)
    {
        FD_ZERO(&socketSet);
        FD_SET(listener, &socketSet);

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = clients[i].socket;

            if (sd > 0)
            {
                FD_SET(sd, &socketSet);
            }

            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &socketSet, NULL, NULL, NULL);
        if (activity < 0)
        {
            continue;
        }

        if (FD_ISSET(listener, &socketSet))
        {
            sd = accept(listener, NULL, NULL);
            if (sd == INVALID_SOCKET)
            {
                printf("Error accepting: %d\n", WSAGetLastError());
            }

            getpeername(sd, (struct sockaddr *)&clientAddr, &clientAddrlen);
            printf("\nClient connected at %s:%d\n",
                   inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            sendRes = send(sd, welcome, welcomeLength, 0);
            if (sendRes != welcomeLength)
            {
                printf("Error sending welcome: %d\n", WSAGetLastError());
            }

            res = recv(sd, clients[curNoClients].username, sizeof(clients[curNoClients].username), 0);
            if (res > 0)
            {
                clients[curNoClients].username[res] = '\0'; // Null-terminate the received username
                printf("Client %s entered the chat.\n", clients[curNoClients].username);

                if (curNoClients >= MAX_CLIENTS)
                {
                    printf("Full\n");

                    sendRes = send(sd, full, fullLength, 0);
                    if (sendRes != fullLength)
                    {
                        printf("Error sending: %d\n", WSAGetLastError());
                    }

                    shutdown(sd, SD_BOTH);
                    closesocket(sd);
                }
                else
                {
                    clients[curNoClients].socket = sd;
                    printf("%s joined the chat.\n", clients[curNoClients].username);
                    curNoClients++;
                }
            }
            else
            {
                printf("Error receiving username: %d\n", WSAGetLastError());
                shutdown(sd, SD_BOTH);
                closesocket(sd);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].socket == 0)
            {
                continue;
            }

            sd = clients[i].socket;
            if (FD_ISSET(sd, &socketSet))
            {
                res = recv(sd, recvbuf, BUFLEN, 0);
                if (res > 0)
                {
                    recvbuf[res] = '\0';
                    printf("\n[%s]: %s\n", clients[i].username, recvbuf);

                    if (memcmp(recvbuf, "/quit", 5 * sizeof(char)) == 0)
                    {
                        printf("%s left the chat.\n", clients[i].username);
                        running = 0;
                        break;
                    }

                    for (int j = 0; j < MAX_CLIENTS; j++)
                    {
                        if (clients[j].socket > 0 && j != i)
                        {
                            // Buffer for modified message
                            char newMessage[BUFLEN + 50]; // increase additional size

                            // Format the new message with the client's name
                            sprintf(newMessage, "%s : %s", clients[i].username, recvbuf); // embed the sender's name

                            // Send the updated message
                            sendRes = send(clients[j].socket, newMessage, strlen(newMessage), 0);
                            if (sendRes == SOCKET_ERROR)
                            {
                                printf("Error broadcasting message: %d\n", WSAGetLastError());
                                shutdown(clients[j].socket, SD_BOTH);
                                closesocket(clients[j].socket);
                                clients[j].socket = 0;
                                curNoClients--;
                            }
                        }
                    }
                }
                else
                {
                    getpeername(sd, (struct sockaddr *)&clientAddr, &clientAddrlen);
                    printf("Client %s disconnected at %s:%d\n",
                           clients[i].username, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

                    shutdown(sd, SD_BOTH);
                    closesocket(sd);
                    clients[i].socket = 0;
                    curNoClients--;
                }
            }
        }
    }

    // CLEANUP ==================================

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].socket > 0)
        {
            sendRes = send(clients[i].socket, goodbye, goodbyeLength, 0);

            shutdown(clients[i].socket, SD_BOTH);
            closesocket(clients[i].socket);
            clients[i].socket = 0;
        }
    }

    closesocket(listener);

    res = WSACleanup();
    if (res)
    {
        printf("Cleanup failed: %d\n", res);
        return 1;
    }

    printf("Shutting down.\nGoodbye.\n");

    return 0;
}

