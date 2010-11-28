all: board.o piece_list.o engine.o main.o
	clang -Wall -g -pedantic -std=c99 -fstack-protector-all -o cerulean board.o piece_list.o engine.o main.o

board.o: board.c board.h move.h piece_list.h piece_list.c
	clang -Wall -g -pedantic -std=c99 -fstack-protector-all -c board.c

piece_list.o: piece_list.h piece_list.c
	clang -Wall -g -pedantic -std=c99 -fstack-protector-all -c piece_list.c

main.o: main.c
	clang -Wall -g -pedantic -std=c99 -fstack-protector-all -c main.c

engine.o: engine.h engine.c board.o
	clang -Wall -g -pedantic -std=c99 -fstack-protector-all -c engine.c

clean: 
	rm *.o cerulean
