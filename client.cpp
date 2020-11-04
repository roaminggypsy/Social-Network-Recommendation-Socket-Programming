#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>

#include <arpa/inet.h>

using namespace std;

#define PORT "33321"
#define MAXDATASIZE 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void query(int sockfd)
{
    //     cout << "Enter country name: ";
    //     char country[MAXDATASIZE];
    //     cin >> country;

    //     cout << "Enter user ID: ";
    //     char id[MAXDATASIZE];
    //     cin >> id;

    //     if (send(sockfd, country, MAXDATASIZE - 1, 0) == -1)
    //     {
    //         perror("send");
    //     }

    //     if (send(sockfd, id, MAXDATASIZE - 1, 0) == -1)
    //     {
    //         perror("send");
    //     }

    //     // send country and id to mainserver
    //     printf("Client has sent User %s and country %s to Main Server using TCP", id, country);
}

int connect()
{
    // load up address structs
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    int status;
    struct addrinfo *res; // pointer to results
    if ((status = getaddrinfo("localhost", PORT, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // loop throgh all the reslts and bind to the first we can
    struct addrinfo *p;
    int sockfd;
    for (p = res; p != NULL; p = p->ai_next)
    {
        // make a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

    freeaddrinfo(res);
    return sockfd;
}

void receive()
{
    // // receive recommendations
    // char res[sizeof(int) + 1];
    // int numbytes;

    // if ((numbytes = recv(sockfd, res, sizeof(int), 0)) == -1)
    // {
    //     perror("recv");
    //     exit(1);
    // }

    // res[numbytes] = '\0';

    // printf("client: received '%s'\n", buf);

    // cout << country << " not found" << endl;
    // cout << "User" << id << " not found" << endl;
    // cout << "Client has received results from Main Server" << endl;
    // cout << "RESULT"
    //      << " is/are possible friend(s) of User" << id << " in " << country << endl;
}

int main()
{
    int sockfd = connect();
    do
    {
        cout << "Enter country name: ";
        char country[MAXDATASIZE];
        cin >> country;

        cout << "Enter user ID: ";
        char id[MAXDATASIZE];
        cin >> id;

        cout << "here" << endl;
        if (send(sockfd, country, MAXDATASIZE - 1, 0) == -1)
        {
            perror("send");
        }
        cout << "there" << endl;

        if (send(sockfd, id, MAXDATASIZE - 1, 0) == -1)
        {
            perror("send");
        }
        cout << "this!" << endl;

        // send country and id to mainserver
        printf("Client has sent User %s and country %s to Main Server using TCP\n", id, country);

        // receive recommendations
        char res[11];
        int numbytes;

        if ((numbytes = recv(sockfd, res, 10, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }

        res[numbytes] = '\0';

        printf("client: received '%s'\n", res);

        //cout << country << " not found" << endl;
        //cout << "User" << id << " not found" << endl;
        //cout << "Client has received results from Main Server" << endl;
        //cout << "RESULT"
        //     << " is/are possible friend(s) of User" << id << " in " << country << endl;
    } while (1);
    close(sockfd);
    return 0;
}