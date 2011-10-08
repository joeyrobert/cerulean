/* book.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "book.h"
#include "board.h"
#include "util.h"

/*
-1 if m1 < m2
0 if m1 == m2
1 if m1 > m2
*/
int compare(const void* pm1, const void* pm2) {
    int i;
	unsigned m1, m2;
    i = 0;
	m1 = *(unsigned*)pm1;
	m2 = *(unsigned*)pm2;

    while(1) {
        /* shorter ones go at the top. all rows end with an EMPTY */
        if(moves[m1][i] == EMPTY)
            return -1;
        if(moves[m2][i] == EMPTY)
            return 1;

        if(moves[m1][i] > moves[m2][i])
            return 1;
        if(moves[m1][i] < moves[m2][i])
            return -1;

        ++i;
    }
}

void generate_opening_book() {
    char line[1000];
    unsigned move;
    char *move_string;
    FILE *file;
    int index, length, line_no, moves_added, i, j;

    line_no = 0;
    file = fopen("BIGBOOK", "r");

    /* Read in seed opening book into moves */
    while(fgets(line, 1000, file) != NULL) {
        index = 0;
        moves_added = 0;
        length = strlen(line);

        while(index < length-1) {
            move_string = substring(line, index, 4);
            move = find_move(move_string);
            board_add(move);
            moves[line_no][moves_added] = move;
            index += 4;
            ++moves_added;
        }

        /* end line with empty */
        moves[line_no][moves_added] = EMPTY;

        for(i = 0; i < moves_added; ++i) {
            board_subtract();
        }

        ++line_no;
    }

    /* sort the moves array */
    for(i = 0; i < 3543; ++i) move_order[i] = i;
    qsort(move_order, 3543, sizeof(unsigned), compare);  
}

unsigned next_opening_move() {
	unsigned i, last_book_move, last_move;
	if(total_history == 0)
		return moves[0][0];

	while(opening_no < 3543) {
		last_book_move = moves[move_order[opening_no]][total_history-1];
		last_move = history[total_history-1][0];

		if(last_move == last_book_move)
			return moves[move_order[opening_no]][total_history];
		
		++opening_no;

		/* check the lower ones are still the same */
		for(i = 0; i < total_history; ++i) {
			if(history[i][0] != moves[move_order[opening_no]][i])
				return EMPTY;
		}
	}

	return EMPTY;
}

/* STORE THE LIST OF OPENING MOVES in sorted ascending order and move down the list with two pointers.
 * if the current move exists as one of the next moves
 * search all nodes that have the last move as their latest move for the next move.
 */
