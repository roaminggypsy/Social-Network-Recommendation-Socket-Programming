# the compiler
CC = g++

# compiler flags:
# -g adds debugging info to the executable file
# -Wall turns on most compiler warnings
CFLAGS = -g -Wall -std=c++11

all: client servermain serverA serverB
.PHONY: all

client: client.o
	$(CC) $(CFLAGS) -o client client.o

client.o: client.cpp
	$(CC) $(CFLAGS) -c client.cpp

servermain: servermain.o
	$(CC) $(CFLAGS) -o servermain servermain.o

servermain.o: servermain.cpp
	$(CC) $(CFLAGS) -c servermain.cpp

serverA: serverA.o
	$(CC) $(CFLAGS) -o serverA serverA.o

serverA.o: serverA.cpp
	$(CC) $(CFLAGS) -c serverA.cpp

serverB: serverB.o
	$(CC) $(CFLAGS) -o serverB serverB.o

serverB.o: serverB.cpp
	$(CC) $(CFLAGS) -c serverB.cpp

clean:
	$(RM) client servermain serverA serverB *.o *~