
/* book.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "book.h"
#include "board.h"
#include "util.h"


char* substring(char *string, int position, int length) {
    char *pointer;
    int c;
 
    pointer = malloc(length+1);
    for(c = 0; c < position; c++) { 
        string++;
    }
 
    for(c = 0; c < length; c++) {
        *(pointer+c) = *string;      
        string++;   
    }
 
    *(pointer+c) = '\0';
    return pointer;
}

/*
-1 if m1 < m2
0 if m1 == m2
1 if m1 > m2
*/
int compare(unsigned m1, unsigned m2) {
    int i;
    i = 0;

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
    unsigned move_bol[385]; /* beginning of line */
    unsigned move;

    char *move_string;
    FILE *file;
    int index, length, line_no, moves_added, i, j;

    line_no = 0;
    file = fopen("BOOK", "r");

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
    printf("STARTED SORTING\n");
    for(i = 0; i < 385; ++i) move_bol[i] = i;
    qsort(move_bol, 385, sizeof(unsigned), compare);
    printf("FINISHED SORTING\n");

    for(i = 0; i < 385; ++i) {
        printf("%08X", move_bol[i]);

        //for(j = 0; j < 50; ++j) {
        //    //if(move_bol[i][j] == EMPTY) break;
      //      //move_to_string(move_bol[i][j], move_string);
    //        printf("%u ", *(move_bol + i*50 + j));
//        }
        printf("\n");
    }
    
    
}

/* STORE THE LIST OF OPENING MOVES in sorted ascending order and move down the list with two pointers.
 * if the current move exists as one of the next moves
 * search all nodes that have the last move as their latest move for the next move.
 */
