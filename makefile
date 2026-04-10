CC = g++
all:
	gcc -c lib/runtime.c -o lib/runtime.o
	g++ -std=c++23 main.cpp lib/runtime.o -I lib/PEGTL/include -lstdc++fs -o a.out

run:
	./a.out