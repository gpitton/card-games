CXX?=g++
SYSINCPATH+=/opt/boost-1.83.0/
WFLAGS=-Wall -Wextra -Weffc++ -Wpedantic -Wno-switch
CXXLANG=c++20
OPTFLAGS=-g -O0
CXXFLAGS+=$(WFLAGS) $(OPTFLAGS) -std=$(CXXLANG) -isystem$(SYSINCPATH)

.PHONY: run-server
run-server: server
	./server 127.0.0.1 8000 .

.PHONY: run-async
run-async: server-async
	./server-async 127.0.0.1 8080 . 1

.PHONY: compile-commands
compile-commands:
	clang++ -MJ compile_commands.json -g -Og -Wall -Wextra -Weffc++ -Wpedantic -Wno-switch -std=c++20 -isystem/opt/boost-1.83.0/ -o server server.cc

.PHONY: clean
clean:
	rm -f server
	rm -f server-async
