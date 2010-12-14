all: board.o piece_list.o search.o util.o hash_table.o zobrist.o mt64.o xboard.o perft.o evaluate.o test.o main.o
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -o cerulean board.o piece_list.o search.o util.o hash_table.o zobrist.o mt64.o xboard.o perft.o evaluate.o test.o main.o

board.o: board.c board.h move.h piece_list.h piece_list.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c board.c

piece_list.o: piece_list.h piece_list.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c piece_list.c

mt64.o: mt64.h mt64.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c mt64.c

zobrist.o: zobrist.h zobrist.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c zobrist.c

hash_table.o: hash_table.h hash_table.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c hash_table.c

main.o: main.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c main.c

util.o: util.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c util.c

search.o: search.h search.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c search.c

perft.o: perft.h perft.c
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c perft.c

xboard.o: xboard.c xboard.h
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c xboard.c

evaluate.o: evaluate.c evaluate.h
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c evaluate.c

test.o: test.c test.h
	gcc -Wall -Wextra -std=c99 -pedantic -O3 -c test.c

clean: 
	rm *.o cerulean
