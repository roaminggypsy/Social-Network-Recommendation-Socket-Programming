# the compiler
CC = g++

# compiler flags:
# -g adds debugging info to the executable file
# -Wall turns on most compiler warnings
CFLAGS = -g -Wall -std=c++11

all: client.cpp servermain.cpp serverA.cpp serverB.cpp
	$(CC) $(CFLAGS) -o client client.cpp
	$(CC) $(CFLAGS) -o serverA serverA.cpp
	$(CC) $(CFLAGS) -o serverB serverB.cpp
	$(CC) $(CFLAGS) -o servermain servermain.cpp


.PHONY: serverA
serverA: 
	./serverA

.PHONY: serverB
serverB: 
	./serverB

.PHONY: mainserver
mainserver: 
	./servermain

clean:
	$(RM) client servermain serverA serverB *.o *~