CXX=g++
INCPATH=/opt/boost-1.83.0/
CXXLANG=c++20
CXXFLAGS=-Wall -Wextra -Weffc++ -Wpedantic -Wno-switch

.PHONY: all
all: server-debug
	$(info incpath: $(INCPATH))

.PHONY: server-debug
server-debug:
	$(CXX) -g -Og $(CXXFLAGS) -std=$(CXXLANG) -isystem$(INCPATH) -o server server.cc

.PHONY: run-server
run-server: server-debug
	./server 127.0.0.1 8000 .

.PHONY: clean
clean:
	rm -f server
