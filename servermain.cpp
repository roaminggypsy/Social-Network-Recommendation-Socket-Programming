#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <unordered_map>
#include <string>
#include <iostream>

using namespace std;

#define APORT "30321"
#define BPORT "31321"
#define PORTFORBACKEND "32321"
#define PORTFORCLIENT "33321"
#define BACKLOG 10
#define MAXDATASIZE 100
#define MAXCOUNTRYLENGTH 22 // Sever Num (0 or 1) + 20 letters + '\0'

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int initialize()
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    int status;
    struct addrinfo *res; // pointer to result
    if ((status = getaddrinfo(NULL, PORTFORCLIENT, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // loop throgh all the reslts and bind to the first we can
    struct addrinfo *p;
    int yes = 1;
    int sockfd;
    for (p = res; p != NULL; p = p->ai_next)
    {
        // make a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        // bind the socket to the port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    //printf("server: waiting fo connections\n");
    return sockfd;
}

int handleClients(int sockfdForClients, int sockfds[], struct addrinfo *addrs[], unordered_map<string, int> country_map, int sockfdForBackend)
{
    while (1)
    {
        struct sockaddr_storage their_addr; // connector's address info
        socklen_t sin_size = sizeof their_addr;
        int new_fd = accept(sockfdForClients, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

        if (!fork())
        {
            // this is child process
            // close(sockfd); // child doesn't need the listener
            int numbytes;
            char country[MAXDATASIZE];
            if ((numbytes = recv(new_fd, country, MAXDATASIZE - 1, 0)) == -1)
            {
                perror("recv");
            }
            country[numbytes] = '\0';
            char id[MAXDATASIZE];
            if ((numbytes = recv(new_fd, id, MAXDATASIZE - 1, 0)) == -1)
            {
                perror("recv");
            }
            id[numbytes] = '\0';
            printf("servermain: received %s, %s\n", country, id);

            unordered_map<string, int>::const_iterator res = country_map.find(country);

            if (res == country_map.end())
            {
                cout << "not found" << endl;
                string msg = string(country) + "does not show up in server A&B";
                if (send(new_fd, msg.data(), msg.length(), 0) == -1)
                {
                    perror("send");
                }
            }
            else
            {
                cout << "found" << endl;
                string query = string(country) + "," + string(id);
                int serverId = res->second;
                if (sendto(sockfds[serverId], query.data(), query.length(), 0, addrs[serverId]->ai_addr, addrs[serverId]->ai_addrlen) == -1)
                {
                    perror("send");
                }
                // receive res from backend

                // send res back to client
            }

            // send query to backend server

            // if (send(new_fd, "Hello, world!", 13, 0) == -1) {
            //     perror("send");
            // }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
}

int connectBackend()
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    //char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORTFORBACKEND, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    return sockfd;
}

void getCountryMapping(int sockfd, unordered_map<string, int> *country_backend_mapping)
{
    int end = 0;
    int numbytes = 0;
    struct sockaddr_storage their_addr;
    char buf[MAXCOUNTRYLENGTH];
    socklen_t addr_len;

    addr_len = sizeof their_addr;
    while (end != 1)
    {
        if ((numbytes = recvfrom(sockfd, buf, MAXCOUNTRYLENGTH, 0,
                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        buf[numbytes] = '\0';

        if (*buf == 'E')
        {
            end++;
        }
        else
        {
            (*country_backend_mapping)[string(buf + 1)] = *buf - '0';
        }
    }

    // printf("listener: got packet from %s\n",
    //     inet_ntop(their_addr.ss_family,
    //         get_in_addr((struct sockaddr *)&their_addr),
    //         s, sizeof s));
    // printf("listener: packet is %d bytes long\n", numbytes);
    // buf[numbytes] = '\0';
    // printf("listener: packet contains \"%c %s\"\n", *buf, buf + 1);

    for (auto i : *country_backend_mapping)
    {
        cout << i.first << ": " << i.second << endl;
    }
}

void initializeForBackend(int sockfds[], struct addrinfo *addrs[])
{
    string ports[2] = {APORT, BPORT};
    for (int i = 0; i < 2; ++i)
    {
        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;

        if ((rv = getaddrinfo("localhost", ports[i].data(), &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        // loop through all the results and make a socket
        for (p = servinfo; p != NULL; p = p->ai_next)
        {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1)
            {
                perror("talker: socket");
                continue;
            }

            break;
        }

        if (p == NULL)
        {
            fprintf(stderr, "talker: failed to create socket\n");
            exit(2);
        }

        sockfds[i] = sockfd;
        addrs[i] = p;
    }

    // for (auto i : adjLists)
    // {
    //     string msg = to_string(2) + i.first;
    //     if ((numbytes = sendto(sockfd, msg.data(), msg.length(), 0,
    //                            p->ai_addr, p->ai_addrlen)) == -1)
    //     {
    //         perror("talker: sendto");
    //         exit(1);
    //     }
    //     else
    //     {
    //         cout << numbytes;
    //     }
    // }

    // if ((numbytes = sendto(sockfd, "E", 1, 0,
    //                        p->ai_addr, p->ai_addrlen)) == -1)
    // {
    //     perror("talker: sendto");
    //     exit(1);
    // }

    // freeaddrinfo(servinfo);

    // //printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    // close(sockfd);
}

int main(void)
{
    unordered_map<string, int> country_backend_mapping;
    // ask backend sever A, B for country mapping
    int backSockfd = connectBackend();
    getCountryMapping(backSockfd, &country_backend_mapping);

    int sockfds[2];
    struct addrinfo *addrs[2];
    initializeForBackend(sockfds, addrs);

    // Establish TCP connection with clients
    // Reference: Beej's Guide
    // TODO: error checking
    int sockfdforclients = initialize();
    handleClients(sockfdforclients, sockfds, addrs, country_backend_mapping, backSockfd);

    // TODO: free all addrinfo
    close(backSockfd);
    close(sockfdforclients);

    return 0;

    // receive country name and user ID

    // request backend server for recommendations

    // case 1: country name or user ID not found
}