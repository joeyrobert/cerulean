all: board.o piece_list.o engine.o util.o main.o
	gcc -Wall -pedantic -std=c99 -O2 -o cerulean-intel board.o piece_list.o engine.o util.o main.o

board.o: board.c board.h move.h piece_list.h piece_list.c
	icl -Wall -pedantic -std=c99 -O2 -c board.c

piece_list.o: piece_list.h piece_list.c
	icl -Wall -pedantic -std=c99 -O2 -c piece_list.c

main.o: main.c
	icl -Wall -pedantic -std=c99 -O2 -c main.c

util.o: util.c
	icl -Wall -pedantic -std=c99 -O2 -c util.c

engine.o: engine.h engine.c board.o
	icl -Wall -pedantic -std=c99 -O2 -c engine.c

clean: 
	rm *.o cerulean
