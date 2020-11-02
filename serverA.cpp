#include <iostream>
#include <fstream>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

#define MAINSERVERPORT "32321"
#define APORT "30321"

void constructGraph(unordered_map<string, unordered_map<int, unordered_set<int>>> *adjLists)
{
    ifstream dataFile;
    dataFile.open("./testcases/testcase1/data1.txt");
    if (dataFile.is_open())
    {
        string line;
        string country;
        while (getline(dataFile, line))
        {
            if (isdigit(line.at(0)))
            {
                stringstream stream(line);
                int node;
                stream >> node;
                //cout << node << ": ";

                unordered_set<int> neighbors;
                (*adjLists)[country][node] = neighbors;
                int neighbor;
                while (stream >> neighbor)
                {
                    (*adjLists)[country][node].insert(neighbor);
                    //cout << neighbor << " ";
                }

                //cout << endl;
            }
            else
            {
                country = line;
                unordered_map<int, unordered_set<int>> adjList;
                (*adjLists)[country] = adjList;
                //cout << country << endl;
            }
        }

        // for (auto i : adjLists[country]) {
        //     cout << i.first << ": ";
        //     for (auto j : adjLists[country][i.first]) {
        //         cout << j << "  ";
        //     }
        //     cout << endl;
        // }
        dataFile.close();
    }
    else
    {
        perror("Can't open data2.txt");
    }
}

int bootup()
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, APORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
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
        exit(2);
    }

    freeaddrinfo(servinfo);
    cout << "The server A is up and running using UDP on port 30321" << endl;
    return sockfd;
}

// Initialize socket for sending to main server
int initializeForMainServer(struct addrinfo **res)
{
    int sockfd;
    struct addrinfo hints, *servinfo;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo("localhost", MAINSERVERPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    struct addrinfo *p;
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

    *res = p;
    cout << p << " " << *res;

    //freeaddrinfo(servinfo);

    return sockfd;
}

void sendCountryList(int sockfd, unordered_map<string, unordered_map<int, unordered_set<int>>> adjLists, struct addrinfo *p)
{
    cout << p;
    int numbytes;
    for (auto i : adjLists)
    {
        string msg = to_string(0) + i.first;
        if ((numbytes = sendto(sockfd, msg.data(), msg.length(), 0,
                               p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
        else
        {
            cout << numbytes;
        }
    }

    if ((numbytes = sendto(sockfd, "E", 1, 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }

    cout << "The server A has sent a country list to Main Server" << endl;
}

void listen(int sockfd)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int numbytes;
    //TODO: find out max length
    char buf[100];
    if ((numbytes = recvfrom(sockfd, buf, 99, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }
    buf[numbytes] = '\0';
    printf("%s", buf);
}

int main()
{
    int listenSockfd = bootup();
    unordered_map<string, unordered_map<int, unordered_set<int>>> adjLists;
    constructGraph(&adjLists);

    struct addrinfo *p;
    int sendSockfd = initializeForMainServer(&p);

    sendCountryList(sendSockfd, adjLists, p);

    listen(listenSockfd);

    //printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(listenSockfd);
    close(sendSockfd);

    return 0;
}