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
#include <list>
#include <cstdlib>

using namespace std;

#define APORT "30321"
#define BPORT "31321"
#define PORTFORBACKEND "32321"
#define PORTFORCLIENT "33321"

#define BACKLOG 10
#define MAXDATASIZE 100
#define MAXCOUNTRYLENGTH 22 // Sever Num (0 or 1) + 20 letters + '\0

#define COUNTRYNOTFOUND -3
#define USERNOTFOUND -2
#define NONE -1

#define SERVERA 0
#define SERVERB 1
#define NUMSERVER 2

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

void handleClients(int sockfdForClients, int sockfds[], struct addrinfo *addrs[],
                   unordered_map<string, int> *country_map,
                   int sockfdForBackend)
{
    int clientCount = 0;
    while (1)
    {
        struct sockaddr_storage their_addr; // connector's address info
        socklen_t sin_size = sizeof their_addr;
        int new_fd = accept(sockfdForClients, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
        }
        clientCount++;

        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        if (!fork())
        {
            int clientNum = clientCount;
            // this is child process
            close(sockfdForClients); // child doesn't need the listener
            while (1)
            {
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
                cout << "The Main server has recevied the request on User " << id << " in " << country
                     << "from client " << clientNum << " using TCP over port " << PORTFORCLIENT << endl;

                unordered_map<string, int>::const_iterator got = (*country_map).find(string(country));
                if (got == (*country_map).end())
                {
                    cout << country << " does not show up in server A&B" << endl;
                    string res = to_string(COUNTRYNOTFOUND);
                    if (send(new_fd, res.data(), res.length(), 0) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }
                    cout << "The Main Server has sent \"Country Name " << country << ": Not found\""
                         << " to client " << clientNum << " using TCP over port " << PORTFORCLIENT << endl;
                }
                else
                {
                    string server = ((got->second) == SERVERA ? "A" : "B");
                    cout << country << " shows up in server " << server << endl;
                    string query = string(country) + "," + string(id);
                    if ((numbytes = sendto(sockfds[got->second], query.data(), query.length(), 0,
                                           addrs[got->second]->ai_addr, addrs[got->second]->ai_addrlen)) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }
                    cout << "The Main Server has sent request from User " << id << " to server " << server
                         << " using UDP over port " << PORTFORBACKEND << endl;

                    // receive res from backend
                    char res[11];
                    struct sockaddr_storage their_addr;
                    socklen_t addr_len = sizeof their_addr;
                    if ((numbytes = recvfrom(sockfdForBackend, res, 10, 0,
                                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
                    {
                        perror("recvfrom");
                        exit(1);
                    }
                    res[numbytes] = '\0';
                    int resVal = atoi(res);
                    if (resVal == USERNOTFOUND)
                    {
                        cout << "The Main server has received \"User ID " << id << ": Not found\""
                             << "from server " << server << endl;
                        // send res back to client
                        if (send(new_fd, res, numbytes, 0) == -1)
                        {
                            perror("send");
                        }
                        cout << "The Main server has sent error to client using TCP over port "
                             << PORTFORCLIENT << endl;
                    }
                    else
                    {
                        cout << "The Main server has received searching result of User "
                             << (resVal == NONE ? "NONE" : res)
                             << " from sever " + server << endl;
                        // send res back to client
                        if (send(new_fd, res, numbytes, 0) == -1)
                        {
                            perror("send");
                        }
                        cout << "The Main Server has sent search result "
                             << "to client " << clientNum
                             << " using TCP over port " << PORTFORCLIENT << endl;
                    }
                }
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
}

int initializeForListeningToBackend()
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
    return sockfd;
}

void getCountryMap(unordered_map<string, int> *country_backend_mapping, int talkToBackfds[], int listenToBackfd,
                   struct addrinfo *backAddrs[])
{
    // request
    int numbytes = 0;
    string request = "GETLIST";

    struct sockaddr_storage their_addr;
    char buf[MAXCOUNTRYLENGTH];
    socklen_t addr_len = sizeof their_addr;

    for (int i = 0; i < 2; i++)
    {
        if ((numbytes = sendto(talkToBackfds[i], request.data(), request.length(), 0,
                               backAddrs[i]->ai_addr, backAddrs[i]->ai_addrlen)) == -1)
        {
            perror("Error in main server asking A for country map: ");
            exit(1);
        }

        bool finished = false;
        while (!finished)
        {
            if ((numbytes = recvfrom(listenToBackfd, buf, MAXCOUNTRYLENGTH, 0,
                                     (struct sockaddr *)&their_addr, &addr_len)) == -1)
            {
                perror("recvfrom");
                exit(1);
            }
            buf[numbytes] = '\0';

            if (string(buf) == "END")
            {
                finished = true;
            }
            else
            {
                (*country_backend_mapping)[string(buf + 1)] = *buf - '0';
            }
        }
        cout << "The Main server has received the country list from server "
             << (i == SERVERA ? "A" : "B")
             << " using UDP over port " << PORTFORBACKEND << endl;
    }

    list<string> countriesInA;
    list<string> countriesInB;
    for (auto it = (*country_backend_mapping).cbegin(); it != (*country_backend_mapping).end(); ++it)
    {
        if (it->second == SERVERA)
        {
            countriesInA.push_back(it->first);
        }
        else
        {
            countriesInB.push_back(it->first);
        }
    }

    cout << "Server A" << endl;
    for (auto it = countriesInA.cbegin(); it != countriesInA.end(); ++it)
    {
        cout << *it << endl;
    }

    cout << endl;

    cout << "Server B" << endl;
    for (auto it = countriesInB.cbegin(); it != countriesInB.end(); ++it)
    {
        cout << *it << endl;
    }
}

void initializeForTalkingToBackend(int sockfds[], struct addrinfo *addrs[])
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
    // Initialization
    int listenToBackSockfd = initializeForListeningToBackend();

    int talkToBackSockfds[2];
    struct addrinfo *backAddrs[2];
    initializeForTalkingToBackend(talkToBackSockfds, backAddrs);

    int listenToClientSockfd = initialize();

    cout << "The Main server is up and running" << endl;

    // Get <country, server> mapping
    unordered_map<string, int> country_backend_mapping;
    getCountryMap(&country_backend_mapping, talkToBackSockfds, listenToBackSockfd, backAddrs);

    // Handle queries from clients
    handleClients(listenToClientSockfd, talkToBackSockfds, backAddrs, &country_backend_mapping, listenToBackSockfd);

    // Cleanup TODO: free all addrinfo and
    close(listenToBackSockfd);
    close(listenToClientSockfd);

    return 0;
}