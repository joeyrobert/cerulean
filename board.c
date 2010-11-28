#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "board.h"
#include "move.h"
#include "piece_list.h"
#include "util.h"

/* piece deltas */
int delta_knight[8] = {14, 31, 33, 18, -14, -31, -33, -18};
int delta_king[8] = {-17, -16, -15, -1, 1, 15, 16, 17};
int delta_diagonal[4] = {15, 17, -15, -17};
int delta_vertical[4] = {16, -16, 1, -1};

void board_new() {
    memset(pieces,  EMPTY, 128);
    memset(colours, EMPTY, 128);
    turn = WHITE;
    castling = 0;
    en_passant_target = NO_EN_PASSANT;
    half_move_clock = 0, full_move_number = 1;
    memset(history, EMPTY, 1024 * 4);
    total_history = 0;

    /* piece list structure */
    w_king = EMPTY;
    b_king = EMPTY;
    memset(&w_pieces, EMPTY, 145*sizeof(unsigned));
    memset(&b_pieces, EMPTY, 145*sizeof(unsigned));
}

void board_set_fen(char* fen) {
    board_new();
    unsigned row = 7, column = 0, colour, skip = 0, iter;

    /* Board and piece lists */
    while(*fen != ' ') {
        if(*fen == '/') { row--; fen++; column = 0; skip = 0; continue;  }
        if(skip > 0) { skip--; fen++; column = (column + 1) % 8; continue; }
        int number = *fen - '0';
        if(number > 0 && number <= 8) { skip = number - 1; fen++; column = (column + 1) % 8; continue; }

        if (*fen == tolower(*fen)) {
            colour = BLACK;
            piece_list_add(&b_pieces, ROWCOLUMN2INDEX(row, column));
        } else {
            colour = WHITE;
            piece_list_add(&w_pieces, ROWCOLUMN2INDEX(row, column));
        }

        colours[ROWCOLUMN2INDEX(row, column)] = colour;
        pieces[ROWCOLUMN2INDEX(row, column)] = piece_name_to_value(tolower(*fen));
        fen++; column = (column + 1) % 8;
    }

    /* Turn */
    fen++;
    if(*fen == 'b') turn = BLACK; /* default white */
    fen += 2;

    /* Castling */
    while(*fen != ' ' && *fen != '-') {
        switch(*fen) {
            case 'K':
                castling |= CASTLE_WK;
                break;
            case 'Q':
                castling |= CASTLE_WQ;
                break;
            case 'k':
                castling |= CASTLE_BK;
                break;
            case 'q':
                castling |= CASTLE_BQ;
                break;
        }
        fen++;
    }

    /* En passant target */
    fen++;
    if(*fen != '-') {
        char ep[2]; ep[0] = *fen; fen++; ep[1] = *fen;
        en_passant_target = piece_to_index(ep);
    }

    /* Halfmove clock */
    fen += 2;
    half_move_clock = atoi(fen);

    /* Fullmove number */
    while(*fen != ' ') fen++; fen++;
    full_move_number = atoi(fen);

    /* King positions */
    iter = 0;
    for(iter = 0; iter < w_pieces.count; iter++) {
        if(pieces[w_pieces.index[iter]] == KING) {
            w_king = w_pieces.index[iter];
            break;
        }
    } 

    for(iter = 0; iter < b_pieces.count; iter++) {
        if(pieces[b_pieces.index[iter]] == KING) {
            b_king = b_pieces.index[iter];
            break;
        }
    } 
}

void board_draw() {
    int row, column;
    printf("\n  +-------------------------------+\n");
    for(row = 7; row >= 0; row--) {
        printf("%d |", row + 1);
        for(column = 0; column < 8; column++) {
            char piece = piece_value_to_name(pieces[ROWCOLUMN2INDEX(row, column)]);
            if (colours[ROWCOLUMN2INDEX(row, column)] == WHITE)
                piece = toupper(piece);
            printf(" %c |", piece);
        }
        printf("\n");
        printf("  +-------------------------------+\n");
    }
    printf("    A   B   C   D   E   F   G   H \n\n");

    char ep[3];
    index_to_piece(en_passant_target, ep);
    printf("Turn:              %i\n", turn);
    printf("Castling:          0x%X\n", castling);
    printf("En Passant Target: %u (%s)\n", en_passant_target, ep);
    printf("Half move number:  %u\n", half_move_clock);
    printf("Full move number:  %u\n", full_move_number);
    printf("W King: %u, B King: %u \n", w_king, b_king);

    int i;
    printf("White pieces: ");
    for(i = 0; i < w_pieces.count; i++) {
        if(w_pieces.index[i] != OFF) {
            index_to_piece(w_pieces.index[i], ep);
            printf("%c (%s) ", piece_value_to_name(pieces[w_pieces.index[i]]), ep);
        }
    }
    printf("\n");

    printf("Black pieces: ");
    for(i = 0; i < b_pieces.count; i++) {
        if(b_pieces.index[i] != EMPTY) {
            index_to_piece(b_pieces.index[i], ep);
            printf("%c (%s) ", piece_value_to_name(pieces[b_pieces.index[i]]), ep);
        }
    }
    printf("\n");
}

unsigned gen_moves(unsigned* moves) {

}

/* updates board and board list */
unsigned board_add(unsigned move) {
}

void board_subtract() {
}
