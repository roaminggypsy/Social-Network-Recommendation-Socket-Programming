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

#define COUNTRYNOTFOUND -3
#define USERNOTFOUND -2
#define NONE -1

// get sockaddr, IPv4 or IPv6 (from Beej's guide)
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int connect()
{
    // This method has code from Beej's guide
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

int main()
{
    // this method has code from Beej's guide
    int sockfd = connect();
    cout << "The client is up and running" << endl;
    do
    {
        cout << "-----Start a new request-----" << endl;
        cout << "Enter country name: ";
        char country[MAXDATASIZE];
        cin >> country;

        cout << "Enter user ID: ";
        char id[MAXDATASIZE];
        cin >> id;

        if (send(sockfd, country, MAXDATASIZE - 1, 0) == -1)
        {
            perror("send");
        }

        if (send(sockfd, id, MAXDATASIZE - 1, 0) == -1)
        {
            perror("send");
        }
        cout << "The client has sent User " << id << " and " << country
             << " to Main Server using TCP" << endl;

        // receive recommendations
        char res[11];
        int numbytes;

        if ((numbytes = recv(sockfd, res, 10, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }

        res[numbytes] = '\0';

        int resVal = atoi(res);
        if (resVal == COUNTRYNOTFOUND)
        {
            cout << country << " not found" << endl;
        }
        else if (resVal == USERNOTFOUND)
        {
            cout << "User " << id << " not found" << endl;
        }
        else
        {
            cout << "The client has received the result from Main Server:" << endl;
            cout << "User ";
            if (resVal == NONE)
            {
                cout << "None";
            }
            else
            {
                cout << resVal;
            }
            cout << " is a possible friend of User " << id
                 << " in " << country << endl;
        }
    } while (1);
    close(sockfd);
    return 0;
}