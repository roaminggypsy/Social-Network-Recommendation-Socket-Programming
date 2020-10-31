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

#define PORT "32321"

void constructGraph(unordered_map<string, unordered_map<int, unordered_set<int>>> *adjLists) 
{
    ifstream dataFile;
    dataFile.open("./testcases/testcase1/data1.txt");
    if (dataFile.is_open()) {
        string line;
        string country;
        while (getline(dataFile, line)) {
            if (isdigit(line.at(0))) {
                stringstream stream(line);
                int node;
                stream >> node;
                //cout << node << ": ";

                unordered_set<int> neighbors;
                (*adjLists)[country][node] = neighbors;
                int neighbor;
                while (stream >> neighbor)  {
                    (*adjLists)[country][node].insert(neighbor);
                    //cout << neighbor << " ";
                }

                //cout << endl;
            } else {
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
    } else {
        perror("Can't open data2.txt");
    }
}

int main()
{
    unordered_map<string, unordered_map<int, unordered_set<int>>> adjLists;
    constructGraph(&adjLists);

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo("localhost", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    
    for (auto i : adjLists) {
        string msg = to_string(1) + i.first;
        if ((numbytes = sendto(sockfd, msg.data(), msg.length(), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
            perror("talker: sendto");
            exit(1);
        } else {
            cout << numbytes;
        }
    }

    if ((numbytes = sendto(sockfd, "E", 1, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
            perror("talker: sendto");
            exit(1);
    }

    freeaddrinfo(servinfo);

    //printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(sockfd);
   
    return 0;
}