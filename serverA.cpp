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
#include <vector>

using namespace std;

#define MAINSERVERPORT "32321"
#define APORT "30321"
#define NOTFOUND -2
#define NONE -1

void constructGraph(unordered_map<string, unordered_map<int, unordered_set<int>>> *countryGraphs)
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
                (*countryGraphs)[country][node] = neighbors;
                int neighbor;
                while (stream >> neighbor)
                {
                    (*countryGraphs)[country][node].insert(neighbor);
                    //cout << neighbor << " ";
                }

                //cout << endl;
            }
            else
            {
                country = line;
                unordered_map<int, unordered_set<int>> adjList;
                (*countryGraphs)[country] = adjList;
                //cout << country << endl;
            }
        }

        // for (auto i : countryGraphs[country]) {
        //     cout << i.first << ": ";
        //     for (auto j : countryGraphs[country][i.first]) {
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
    cout << p << " " << *res << endl;

    //freeaddrinfo(servinfo);

    return sockfd;
}

void sendCountryList(int sockfd, unordered_map<string, unordered_map<int, unordered_set<int>>> *countryGraphs, struct addrinfo *p)
{
    cout << p;
    int numbytes;
    for (auto i : *countryGraphs)
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

int recommend(unordered_map<int, unordered_set<int>> graph, int u)
{
    if (graph.count(u) == 0)
    {
        return NOTFOUND;
    }

    vector<int> users;
    for (auto adjList : graph)
    {
        users.push_back(adjList.first);
    }

    bool hasAns = false, hasOpt = false;
    int optId, optNumCommonNeighbor = 0;        // node that has common neighbors
    int suboptId = INT32_MAX, suboptDegree = 0; // node that has no common neighbors
    for (const auto &v : users)
    {
        if (graph[u].count(v) == 0)
        {
            // v is not connected to u
            hasAns = true;
            int numCommonNeighbor = 0;
            for (const auto &vNeighbor : graph[v])
            {
                if (graph[u].count(vNeighbor) > 0)
                {
                    numCommonNeighbor++;
                }
            }

            if (numCommonNeighbor == 0)
            {
                if (!hasOpt)
                {
                    // No nodes have common neighbors so far
                    int degree = graph[v].size();
                    if (degree > suboptDegree)
                    {
                        suboptId = v;
                        suboptDegree = degree;
                    }
                    else if (degree == suboptDegree && v < suboptId)
                    {
                        suboptId = v;
                    }
                }
            }
            else
            {
                if (numCommonNeighbor > optNumCommonNeighbor)
                {
                    hasOpt = true;
                    optNumCommonNeighbor = numCommonNeighbor;
                    optId = v;
                }
                else if (numCommonNeighbor == optNumCommonNeighbor && v < optId)
                {
                    optId = v;
                }
            }
        }
    }

    if (hasAns)
    {
        if (hasOpt)
        {
            return optId;
        }
        else
        {
            return suboptId;
        }
    }
    else
    {
        return NONE;
    }
}

void listenToMainServer(int listenSockfd,
                        unordered_map<string, unordered_map<int, unordered_set<int>>> *countryGraphs,
                        int talkSockfd, struct addrinfo *p)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int numbytes;
    //TODO: find out max length
    char buf[100];
    if ((numbytes = recvfrom(listenSockfd, buf, 99, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }
    buf[numbytes] = '\0';

    int commaIdx = 0;
    while (buf[commaIdx] != ',')
    {
        commaIdx++;
    }

    string country(buf, commaIdx);
    int u = atoi(buf + commaIdx + 1);
    //cout << country << " " << userId;

    int res = recommend((*countryGraphs)[country], u);
    char *tosend = (char *)&res;
    int size = sizeof(res);
    if ((numbytes = sendto(talkSockfd, tosend, size, 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }
}

int main()
{
    int listenSockfd = bootup();
    unordered_map<string, unordered_map<int, unordered_set<int>>> countryGraphs;
    constructGraph(&countryGraphs);

    struct addrinfo *p;
    int talkSockfd = initializeForMainServer(&p);

    sendCountryList(talkSockfd, &countryGraphs, p);

    listenToMainServer(listenSockfd, &countryGraphs, talkSockfd, p);

    //printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(listenSockfd);
    close(talkSockfd);

    return 0;
}