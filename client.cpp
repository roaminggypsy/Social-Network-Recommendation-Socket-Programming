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

void query()
{
    cout << "Enter country name: ";
    char country[MAXDATASIZE];
    cin >> country;

    cout << "Enter user ID: ";
    char id[MAXDATASIZE];
    cin >> id;

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
        return 1;
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
        return 2;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

    freeaddrinfo(res);

    if (send(sockfd, country, MAXDATASIZE - 1, 0) == -1)
    {
        perror("send");
    }

    if (send(sockfd, id, MAXDATASIZE - 1, 0) == -1)
    {
        perror("send");
    }
}

int main()
{
    do
    {

        // receive recommendations
        // int numbytes;
        // char buf[MAXDATASIZE];
        // if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        //     perror("recv");
        //     exit(1);
        // }

        // buf[numbytes] = '\0';

        // printf("client: received '%s'\n", buf);

        close(sockfd);

        // send country and id to mainserver
        // cout << "Client has sent User" << id << " and " << country << " to Main Server using TCP" << endl;

        // print result
        // cout << country << " not found" << endl;
        // cout << "User" << id << " not found" << endl;
        // cout << "Client has received results from Main Server" << endl;
        // cout << "RESULT" << " is/are possible friend(s) of User" << id << " in " << country << endl;
    } while (1);

    return 0;
}