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
#include <limits>

using namespace std;

#define MAINSERVERPORT "32321"
#define BPORT "31321"
#define USERNOTFOUND -2
#define NONE -1
#define SERVER 'B'

void constructGraph(unordered_map<string, unordered_map<int32_t, unordered_set<int32_t>>> *countryGraphs)
{
    ifstream dataFile;
    dataFile.open("./data2.txt");
    if (dataFile.is_open())
    {
        string line;
        string country;
        while (getline(dataFile, line))
        {
            stringstream stream(line);
            string first;
            stream >> first;
            if (isdigit(first.at(0)))
            {
                stringstream stream(line);
                int32_t node = atoi(first.data());

                unordered_set<int32_t> neighbors;
                (*countryGraphs)[country][node] = neighbors;
                int32_t neighbor;
                while (stream >> neighbor)
                {
                    (*countryGraphs)[country][node].insert(neighbor);
                }
            }
            else
            {
                country = first;
                unordered_map<int32_t, unordered_set<int32_t>> adjList;
                (*countryGraphs)[country] = adjList;
            }
        }
        dataFile.close();
    }
    else
    {
        perror("Can't open data2.txt");
    }
}

int bootup()
{
    // This method has code from Beej's guide
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, BPORT, &hints, &servinfo)) != 0)
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
    return sockfd;
}

// Initialize socket for sending to main server
int initializeForMainServer(struct addrinfo **res)
{
    // This method has code from Beej's guide
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

    //freeaddrinfo(servinfo);

    return sockfd;
}

void sendCountryList(int sockfd, unordered_map<string, unordered_map<int32_t, unordered_set<int32_t>>> *countryGraphs, struct addrinfo *p)
{
    // This method has code from Beej's guide
    int numbytes;
    for (auto i : *countryGraphs)
    {
        string msg = to_string(1) + i.first;
        if ((numbytes = sendto(sockfd, msg.data(), msg.length(), 0,
                               p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
    }

    if ((numbytes = sendto(sockfd, "END", 3, 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }

    cout << "The server " << SERVER << " has sent a country list to Main Server" << endl;
}

int32_t recommend(unordered_map<int32_t, unordered_set<int32_t>> *graph, int32_t u, string country)
{
    if (graph->count(u) == 0)
    {
        cout << "User " << u << " does not show up in " << country << endl;
        return USERNOTFOUND;
    }

    cout << "The server " << SERVER << " is searching possible friends for User " << u << " ..." << endl;
    vector<int32_t> users;
    for (auto adjList : *graph)
    {
        users.push_back(adjList.first);
    }

    bool hasAns = false, hasOpt = false;
    int32_t optId = INT32_MAX, optNumCommonNeighbor = 0; // node that has common neighbors
    int32_t suboptId = INT32_MAX, suboptDegree = 0;      // node that has no common neighbors
    for (const auto &v : users)
    {
        if (v != u && (*graph)[u].count(v) == 0)
        {
            // v is not connected to u
            hasAns = true;
            int numCommonNeighbor = 0;
            for (const auto &vNeighbor : (*graph)[v])
            {
                if ((*graph)[u].count(vNeighbor) > 0)
                {
                    numCommonNeighbor++;
                }
            }

            if (numCommonNeighbor == 0)
            {
                if (!hasOpt)
                {
                    // No nodes have common neighbors so far
                    int degree = (*graph)[v].size();
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

    cout << "Here are the result: User ";
    if (hasAns)
    {
        if (hasOpt)
        {
            cout << optId << endl;
            return optId;
        }
        else
        {
            cout << suboptId << endl;
            return suboptId;
        }
    }
    else
    {
        cout << "NONE" << endl;
        return NONE;
    }
}

void listenToMainServer(int listenSockfd,
                        unordered_map<string, unordered_map<int32_t, unordered_set<int32_t>>> *countryGraphs,
                        int talkSockfd, struct addrinfo *p)
{
    // This method has code from Beej's guide
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

    if (string(buf) == "GETLIST")
    {
        sendCountryList(talkSockfd, countryGraphs, p);
    }
    else
    {
        int commaIdx = 0;
        while (buf[commaIdx] != ',')
        {
            commaIdx++;
        }

        string country(buf, commaIdx);
        int u = atoi(buf + commaIdx + 1);
        cout << endl;
        cout << "The server " << SERVER << " has received request for finding possible friends "
             << "of User " << u << " in " << country << endl;

        int resVal = recommend(&(*countryGraphs)[country], u, country);
        string resStr = to_string(resVal);
        if ((numbytes = sendto(talkSockfd, resStr.data(), resStr.size(), 0, p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
        cout << "The server " << SERVER << " has sent ";
        if (resVal == USERNOTFOUND)
        {
            cout << "\"User " << u << " not found\" ";
        }
        else
        {
            cout << "the result ";
        }
        cout << "to Main Server" << endl;
    }
}

int main()
{
    int listenSockfd = bootup();
    unordered_map<string, unordered_map<int, unordered_set<int>>> countryGraphs;
    constructGraph(&countryGraphs);

    struct addrinfo *p;
    int talkSockfd = initializeForMainServer(&p);

    cout << "The server " << SERVER << " is up and running using UDP on port " << BPORT << endl;

    while (1)
    {
        listenToMainServer(listenSockfd, &countryGraphs, talkSockfd, p);
    }

    close(listenSockfd);
    close(talkSockfd);

    return 0;
}