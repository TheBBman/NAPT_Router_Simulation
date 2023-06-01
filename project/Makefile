COMPILER = g++
CFLAGS = -Wall -ggdb -O0
INCL_PATHS =

all: server

server: main.cpp
	$(COMPILER) -std=c++20 $(INCL_PATHS) $< -o $@ $(CFLAGS)

clean:
	rm -rf server
	rm -rf *.dSYM
