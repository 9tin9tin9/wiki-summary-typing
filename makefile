CC = clang
CXX = clang++
CFLAGS = -g
CXXFLAGS = $(CFLAGS) -std=c++11
LDFLAGS = -lncurses

main: main.o cJSON.o
	$(CXX) *.o $(LDFLAGS) -o main

clean:
	rm *.o
	rm -rf *.dSYM
