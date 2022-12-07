CC = g++
# CFLAGS = -g -Wall -ansi

all: server
server: server.cpp
	$(CC) -o ./build/server server.cpp  -lpthread