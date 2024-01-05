CC := gcc
CXX := g++

CFLAGS := -Wall -Wextra -pedantic -g -O2 -std=gnu17
CXXFLAGS := -Wall -Wextra -pedantic -g -O2 -std=gnu++17 -fno-exceptions -fno-rtti

wadmaker: main.o flags.o zstd.o
	$(CXX) $(CXXFLAGS) $^ -o $@

zstd.o: zstd.c
	$(CC) $(CFLAGS) -c $< -o $@

flags.o: flags.cpp flags.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: main.cpp flags.h common.h zstd.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
