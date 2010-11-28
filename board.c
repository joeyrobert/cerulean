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
    enpassant_target = NO_ENPASSANT;
    half_move_clock = 0, full_move_number = 1;
    memset(history, EMPTY, 1024 * 4);
    total_history = 0;

    /* piece list structure */
    w_king = EMPTY;
    b_king = EMPTY;
    piece_list_new(&w_pieces);
    piece_list_new(&b_pieces);
}

void board_set_fen(char* fen) {
    unsigned row, column, colour, skip, iter, number;
    board_new();
	row = 7; column = 0; skip = 0;

    /* Board and piece lists */
    while(*fen != ' ') {
        if(*fen == '/') { row--; fen++; column = 0; skip = 0; continue;  }
        if(skip > 0) { skip--; fen++; column = (column + 1) % 8; continue; }
        number = *fen - '0';
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
        enpassant_target = piece_to_index(ep);
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
    unsigned i;
	char ep[3];

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

    index_to_piece(enpassant_target, ep);
    printf("Turn:              %i\n", turn);
    printf("Castling:          0x%X\n", castling);
    printf("En Passant Target: %u (%s)\n", enpassant_target, ep);
    printf("Half move number:  %u\n", half_move_clock);
    printf("Full move number:  %u\n", full_move_number);
    printf("W King: %u, B King: %u \n", w_king, b_king);

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
    piece_list *list;
    unsigned i, j, index, count, new_move, last_row, first_row;

    switch(turn) {
    case WHITE:
        list = &w_pieces;
        last_row = 7;
        first_row = 1;
        break;
    case BLACK:
        list = &b_pieces;
        last_row = 0;
        first_row = 6;
        break;
    }

    count = 0;
    for(i = 0; i < list->count; i++) {
        index = list->index[i];
        switch(pieces[index]) {
        case PAWN:
            /* Regular push */
            new_move = index + 16*turn;
            if(LEGAL_MOVE(new_move) && pieces[new_move] == EMPTY) {
                if(INDEX2ROW(new_move) == last_row) {
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_PROMOTE, QUEEN);
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_PROMOTE, ROOK);
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_PROMOTE, BISHOP);
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_PROMOTE, KNIGHT);
                } else {
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE, 0);
                }
            }
            
            /* Double push */
            new_move = index + 32*turn;
            if(LEGAL_MOVE(new_move) && pieces[index+16*turn] == EMPTY && pieces[new_move] == EMPTY && INDEX2ROW(index) == first_row)
                moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_PAWN_DOUBLE, 0);

            for(j = 0; j <= 2; j = j+2) {                
                /* Captures */
                new_move = index + (16 + j - 1)*turn;
                if(LEGAL_MOVE(new_move) && colours[new_move] == -turn) {
                    if(INDEX2ROW(new_move) == last_row) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_CAPTURE | BITS_PROMOTE, QUEEN);
                        moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_CAPTURE | BITS_PROMOTE, ROOK);
                        moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_CAPTURE | BITS_PROMOTE, BISHOP);
                        moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_CAPTURE | BITS_PROMOTE, KNIGHT);
                    } else {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_CAPTURE, 0);
                    }
                }

                /* En passant */
                if(LEGAL_MOVE(new_move) && new_move == enpassant_target) {
                    moves[count++] = MOVE_NEW(index, new_move, BITS_PAWN_MOVE | BITS_ENPASSANT | BITS_CAPTURE, 0);
                }
            }
            break;
        case KNIGHT:
            for(j = 0; j < 8; j++) {
                new_move = index + delta_knight[j];
                if(LEGAL_MOVE(new_move) && colours[new_move] != turn) {
                    if(colours[new_move] == -turn)
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                    else
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);
                }
            }
            break;
        case KING:
            for(j = 0; j < 8; j++) {
                new_move = index + delta_king[j];
                if(LEGAL_MOVE(new_move) && colours[new_move] != turn) {
                    if(colours[new_move] == -turn)
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                    else
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);
                }
            }
            break;
        case BISHOP:
            for(j = 0; j < 4; j++) {
                new_move = index;

                do {
                    new_move += delta_diagonal[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;

                    if(pieces[new_move] == EMPTY) {
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);                        
                    } else if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
            }
            break;
        case ROOK:
            for(j = 0; j < 4; j++) {
                new_move = index;

                do {
                    new_move += delta_vertical[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;

                    if(pieces[new_move] == EMPTY) {
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);                        
                    } else if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
            }
            break;
        case QUEEN:
            for(j = 0; j < 4; j++) {
                new_move = index;
                do {
                    new_move += delta_diagonal[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;

                    if(pieces[new_move] == EMPTY) {
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);                        
                    } else if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
                
                new_move = index;
                do {
                    new_move += delta_vertical[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;

                    if(pieces[new_move] == EMPTY) {
                        moves[count++] = MOVE_NEW(index, new_move, 0, 0);                        
                    } else if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
            }
            break;
        }
    }

    /* TODO: Castling */

    return count;
}

/* updates board and board list */
unsigned board_add(unsigned move) {
    return 0;
}

void board_subtract() {
}
