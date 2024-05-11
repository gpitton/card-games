CC=g++
INCPATH=/opt/boost-1.83.0/
CPPLANG=c++20
CPPFLAGS=-Wall -Wextra -Weffc++ -Wpedantic

server-debug: server
	$(CC) -Og $(CPPFLAGS) -std=$(CPPLANG) -isystem$(INCPATH) -o server server.cc

all:
	server-debug

run-server: server-debug
	./server 127.0.0.1 8000 .
