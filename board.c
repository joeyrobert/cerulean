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

void board_create_table() {
    int size = 21;
    table = (hash_table*) malloc(sizeof(hash_table));
    hash_new(table, size);
}

void board_new() {
	int i;
    memset(pieces,  EMPTY, 128*sizeof(unsigned));
    memset(colours, EMPTY, 128*sizeof(unsigned));
    turn = WHITE;
    castling = 0;
    enpassant_target = NO_ENPASSANT;
    half_move_clock = 0, full_move_number = 1;
    memset(history, EMPTY, sizeof(unsigned) * 1024 * 4);
    total_history = 0;

    /* piece list structure */
    w_king = EMPTY; b_king = EMPTY;
    piece_list_new(&w_pieces);	piece_list_new(&b_pieces);

	for(i = 0; i < 7; i++) {
		piece_list_new(&w_pieces_by_type[i]);
		piece_list_new(&b_pieces_by_type[i]);
	}
}

void board_set_fen(char* fen) {    
    unsigned row, column, colour, skip, i, number;
    board_new();
	row = 7; column = 0; skip = 0;

    /* Board and piece lists */
    while(*fen != ' ') {
        if(*fen == '/') {
            row--;
            fen++;
            column = 0;
            skip = 0;
            continue;
        }

        if(skip > 0){
            skip--;
            column++;
            continue;
        }

        number = *fen - '0';
        if(number > 0 && number <= 8) {
            skip = number - 1;
            fen++;
            column++;
            continue;
        }

        if (*fen == tolower(*fen)) {
            colour = BLACK;
            piece_list_add(&b_pieces, ROWCOLUMN2INDEX(row, column));
        } else {
            colour = WHITE;
            piece_list_add(&w_pieces, ROWCOLUMN2INDEX(row, column));
        }

        colours[ROWCOLUMN2INDEX(row, column)] = colour;
        pieces[ROWCOLUMN2INDEX(row, column)] = piece_name_to_value(tolower(*fen));
        fen++;
        column++;
    }

    /* Turn */
    fen++;
    if(*fen == 'b') turn = BLACK; /* default white */
    fen += 2;

    /* Castling */
    if(*fen != '-') {
        while(*fen != ' ') {
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
    } else
        fen++;

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
    for(i = 0; i < w_pieces.count; i++) {
        if(pieces[w_pieces.index[i]] == KING)
            w_king = w_pieces.index[i];
        piece_list_add(&w_pieces_by_type[pieces[w_pieces.index[i]]], w_pieces.index[i]);
    } 

    for(i = 0; i < b_pieces.count; i++) {
        if(pieces[b_pieces.index[i]] == KING)
            b_king = b_pieces.index[i];
        piece_list_add(&b_pieces_by_type[pieces[b_pieces.index[i]]], b_pieces.index[i]);
    }

    zobrist = board_gen_zobrist();
}

void board_draw() {
    int row, column;
	char ep[3];

    printf("  +---+---+---+---+---+---+---+---+\n");
    for(row = 7; row >= 0; row--) {
        printf("%d |", row + 1);
        for(column = 0; column < 8; column++) {
            char piece = piece_value_to_name(pieces[ROWCOLUMN2INDEX(row, column)]);
            if (colours[ROWCOLUMN2INDEX(row, column)] == WHITE)
                piece = toupper(piece);
            printf(" %c |", piece);
        }
        printf("\n");
        printf("  +---+---+---+---+---+---+---+---+\n");
    }
    printf("    A   B   C   D   E   F   G   H \n\n");

    index_to_piece(enpassant_target, ep);
    printf("Turn:    %i, Castling: 0x%X, En Passant Target: %s\n", turn, castling, ep);
    printf("Half:   %2u, Full:  %2u\n", half_move_clock, full_move_number);
    printf("W King:  %u, B King: %u\n", w_king, b_king);
    printf("Zobrist: 0x%016llX\n", (long long unsigned int)zobrist);
}

MOVTYPE gen_moves(unsigned* moves) {
    piece_list *list;
    unsigned i, j, index, count, new_move, last_row, first_row;

    switch(turn) {
    case WHITE:
        list = &w_pieces;
        last_row = 7;
        first_row = 1;
        break;
    default:
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

    /* Castling */
    switch(turn) {
    case WHITE:
        /* this tries to minimize the times it calls is_attacked */
        if(((castling & CASTLE_WK) || (castling & CASTLE_WQ)) && !is_in_check(WHITE)) {
            if((castling & CASTLE_WQ) && pieces[1] == EMPTY && pieces[2] == EMPTY && pieces[3] == EMPTY && !is_attacked(2, BLACK) && !is_attacked(3, BLACK))
                moves[count++] = MOVE_NEW(4, 2, BITS_CASTLE, 0);
            if((castling & CASTLE_WK) && pieces[5] == EMPTY && pieces[6] == EMPTY && !is_attacked(5, BLACK) && !is_attacked(6, BLACK))
                moves[count++] = MOVE_NEW(4, 6, BITS_CASTLE, 0);
        }
        break;
    case BLACK:
        if(((castling & CASTLE_BK) || (castling & CASTLE_BQ)) && !is_in_check(BLACK)) {
            if((castling & CASTLE_BQ) && pieces[113] == EMPTY && pieces[114] == EMPTY && pieces[115] == EMPTY && !is_attacked(114, WHITE) && !is_attacked(115, WHITE))
                moves[count++] = MOVE_NEW(116, 114, BITS_CASTLE, 0);
            if((castling & CASTLE_BK) && pieces[117] == EMPTY && pieces[118] == EMPTY && !is_attacked(117, WHITE) && !is_attacked(118, WHITE))
                moves[count++] = MOVE_NEW(116, 118, BITS_CASTLE, 0);
        }
        break;
    }

    return count;
}

unsigned gen_caps(unsigned* moves) {
    piece_list *list;
    unsigned i, j, index, count, new_move, last_row;

    switch(turn) {
    case WHITE: /* ugly */
        list = &w_pieces;
        last_row = 7;
        break;
    default:
        list = &b_pieces;
        last_row = 0;
        break;
    }

    count = 0;
    for(i = 0; i < list->count; i++) {
        index = list->index[i];
        switch(pieces[index]) {
        case PAWN:
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
                }
            }
            break;
        case KING:
            for(j = 0; j < 8; j++) {
                new_move = index + delta_king[j];
                if(LEGAL_MOVE(new_move) && colours[new_move] != turn) {
                    if(colours[new_move] == -turn)
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                }
            }
            break;
        case BISHOP:
            for(j = 0; j < 4; j++) {
                new_move = index;

                do {
                    new_move += delta_diagonal[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;
              
                    if(colours[new_move] == -turn) {
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
                     
                    if(colours[new_move] == -turn) {
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

                    if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
                
                new_move = index;
                do {
                    new_move += delta_vertical[j];
                    if(!LEGAL_MOVE(new_move) || colours[new_move] == turn) break;

                    if(colours[new_move] == -turn) {
                        moves[count++] = MOVE_NEW(index, new_move, BITS_CAPTURE, 0);
                        break;
                    }
                } while(1);
            }
            break;
        }
    }

    return count;
}

/* updates board and board list */
unsigned board_add(unsigned move) {
    unsigned from, to;
    from = MOVE2FROM(move);
    to = MOVE2TO(move);
    
    history[total_history][0] = move;
    history[total_history][1] = pieces[to];         /* piece that existed there before,         */
    history[total_history][2] = enpassant_target;   /* will either be empty or -1 * turn colour */
    history[total_history][3] = castling;
    zobrist_history[total_history] = zobrist;
    ++total_history;

    /* REGULAR MOVE */
    if(MOVE2BITS(move) == 0) {
        move_piece(from, to);
        board_enpassant(NO_ENPASSANT);
    /* DOUBLE PAWN */
    } else if(move & BITS_PAWN_DOUBLE) {
        move_piece(from, to);
        board_enpassant(to - 16*turn);
    /* EN PASSANT (must be before capture) */
    } else if(move & BITS_ENPASSANT) {
        switch(turn) {
        case WHITE:
            zobrist ^= zobrist_b[pieces[enpassant_target - turn*16]][enpassant_target - turn*16];
            piece_list_subtract(&b_pieces, enpassant_target - turn*16);
            piece_list_subtract(&b_pieces_by_type[PAWN], enpassant_target - turn*16);
            break;
        case BLACK:
            zobrist ^= zobrist_w[pieces[enpassant_target - turn*16]][enpassant_target - turn*16];
            piece_list_subtract(&w_pieces, enpassant_target - turn*16);
            piece_list_subtract(&w_pieces_by_type[PAWN], enpassant_target - turn*16);
            break;
        }

        pieces[enpassant_target - turn*16] = EMPTY;
        colours[enpassant_target - turn*16] = EMPTY;
        move_piece(from, to);
        board_enpassant(NO_ENPASSANT);
    /* PROMOTE (must be before capture) */
    } else if(move & BITS_PROMOTE) {
        if(move & BITS_CAPTURE)
            board_capture_piece(from, to);
        else
            move_piece(from, to);
        pieces[to] = MOVE2PROMOTE(move);
		  
        switch(turn) {
        case WHITE:
			piece_list_subtract(&w_pieces_by_type[PAWN], to);
			piece_list_add(&w_pieces_by_type[pieces[to]], to);

            zobrist ^= zobrist_w[PAWN][to];
            zobrist ^= zobrist_w[pieces[to]][to];
            break;
        case BLACK:
			piece_list_subtract(&b_pieces_by_type[PAWN], to);
			piece_list_add(&b_pieces_by_type[pieces[to]], to);

            zobrist ^= zobrist_b[PAWN][to];
            zobrist ^= zobrist_b[pieces[to]][to];
            break;
        }

        board_enpassant(NO_ENPASSANT);
    /* CAPTURE */
    } else if(move & BITS_CAPTURE) {
        board_capture_piece(from, to);
        board_enpassant(NO_ENPASSANT);
    /* PAWN MOVE */
    } else if(move & BITS_PAWN_MOVE) {
        move_piece(from, to);
        board_enpassant(NO_ENPASSANT);
    /* CASTLE */
    } else if(move & BITS_CASTLE) {
        move_piece(from, to);
        board_enpassant(NO_ENPASSANT);

        switch(to) {
        case 2:
            move_piece(0, 3);
            break;
        case 6:
            move_piece(7, 5);
            break;
        case 118:
            move_piece(119, 117);
            break;
        case 114:
            move_piece(112, 115);
            break;       
        }
        
        switch(turn) {
        case WHITE:
            if(castling & CASTLE_WQ) {
                castling -= CASTLE_WQ;
                zobrist ^= zobrist_castling[CASTLE_WQ];
            }
            if(castling & CASTLE_WK) {
                castling -= CASTLE_WK;
                zobrist ^= zobrist_castling[CASTLE_WK];
            }
            break;
        case BLACK:
            if(castling & CASTLE_BQ) {
                castling -= CASTLE_BQ;
                zobrist ^= zobrist_castling[CASTLE_BQ];
            }
            if(castling & CASTLE_BK) {
                castling -= CASTLE_BK;
                zobrist ^= zobrist_castling[CASTLE_BK];
            }
            break;
        }
    }

    /* Set the new king location */
    if(pieces[to] == KING) {
        switch(turn) {
        case WHITE:
            w_king = to;
            break;
        case BLACK:
            b_king = to;
            break;
        }
    }
    
    /* Update castling bits */
    if((castling & CASTLE_WQ) && (to == 0 || from == 0 || to == 4 || from == 4)) {
        castling -= CASTLE_WQ;
        zobrist ^= zobrist_castling[CASTLE_WQ];
    }

    if((castling & CASTLE_WK) && (to == 7 || from == 7 || to == 4 || from == 4)) {
        castling -= CASTLE_WK;
        zobrist ^= zobrist_castling[CASTLE_WK];
    }
    if((castling & CASTLE_BQ) && (to == 112 || from == 112 || to == 116 || from == 116)) {
        castling -= CASTLE_BQ;
        zobrist ^= zobrist_castling[CASTLE_BQ];
    }
    if((castling & CASTLE_BK) && (to == 119 || from == 119 || to == 116 || from == 116)) {
        castling -= CASTLE_BK;
        zobrist ^= zobrist_castling[CASTLE_BK];
    }
    
    turn = -1 * turn;
    zobrist ^= zobrist_side;

    /* revert if in check */
    if(is_in_check(-1 * turn)) {
        board_subtract();
        return 0;
    }

    return 1;
}

void board_subtract() {
    unsigned from, to, move, previous_piece;
    turn = -1 * turn;
    --total_history;
    move = history[total_history][0];
    previous_piece = history[total_history][1];     /* piece that existed there before,         */
    enpassant_target = history[total_history][2];   /* will either be empty or -1 * turn colour */
    castling = history[total_history][3];
    zobrist = zobrist_history[total_history];
    
    from = MOVE2FROM(move);
    to = MOVE2TO(move);

    /* REGULAR MOVE, DOUBLE PAWN */
    if(MOVE2BITS(move) == 0 || move & BITS_PAWN_DOUBLE) {
        move_piece_discreetly(to, from);
    /* EN PASSANT (must be before capture) */
    } else if(move & BITS_ENPASSANT) {
        switch(turn) {
        case WHITE:
            piece_list_add(&b_pieces, enpassant_target - turn*16);
            piece_list_add(&b_pieces_by_type[PAWN], enpassant_target - turn*16);
            break;
        case BLACK:
            piece_list_add(&w_pieces, enpassant_target - turn*16);
            piece_list_add(&w_pieces_by_type[PAWN], enpassant_target - turn*16);
            break;
        }
        move_piece_discreetly(to, from);    
        pieces[enpassant_target - turn*16] = PAWN;
        colours[enpassant_target - turn*16] = -1*turn;
    /* PROMOTE (must be before capture) */
    } else if(move & BITS_PROMOTE) {
        move_piece_discreetly(to, from);
		
        switch(turn) {
        case WHITE:
			piece_list_subtract(&w_pieces_by_type[pieces[from]], from);
			piece_list_add(&w_pieces_by_type[PAWN], from);
            break;
        case BLACK:
			piece_list_subtract(&b_pieces_by_type[pieces[from]], from);
			piece_list_add(&b_pieces_by_type[PAWN], from);
            break;
        }
        pieces[from] = PAWN;

		
        if(move & BITS_CAPTURE) {
            switch(turn) {
            case WHITE:
                piece_list_add(&b_pieces, to);
                piece_list_add(&b_pieces_by_type[previous_piece], to);
                break;
            case BLACK:
                piece_list_add(&w_pieces, to);
                piece_list_add(&w_pieces_by_type[previous_piece], to);
                break;
            }
            pieces[to] = previous_piece;
            colours[to] = -1 * turn;
        }
    /* CAPTURE */
    } else if(move & BITS_CAPTURE) {
        switch(turn) {
        case WHITE:
            piece_list_add(&b_pieces, to);
            piece_list_add(&b_pieces_by_type[previous_piece], to);
            break;
        case BLACK:
            piece_list_add(&w_pieces, to);
            piece_list_add(&w_pieces_by_type[previous_piece], to);
            break;
        }
        move_piece_discreetly(to, from);
        pieces[to] = previous_piece;
        colours[to] = -1 * turn;
    /* PAWN PUSH (must be after capture) */
    } else if(move & BITS_PAWN_MOVE) {
        move_piece_discreetly(to, from);
    /* CASTLE */
    } else if(move & BITS_CASTLE) {
        move_piece_discreetly(to, from);

        switch(to) {
        case 2:
            move_piece_discreetly(3, 0);
            break;
        case 6:
            move_piece_discreetly(5, 7);
            break;
        case 118:
            move_piece_discreetly(117, 119);
            break;
        case 114:
            move_piece_discreetly(115, 112);
            break;       
        }
    }

    /* Post processing */
    if(pieces[from] == KING) {
        switch(turn) {
        case WHITE:
            w_king = from;
            break;
        case BLACK:
            b_king = from;
            break;
        }
    }
}

unsigned is_attacked(unsigned index, int by) {
    unsigned new_move, j;
    
    /* Pawns */
    for(j = 0; j <= 2; j = j + 2) {
        new_move = index - (16 * by - 1 + j);
        if (LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == PAWN)
            return 1;
    }

    for(j = 0; j < 4; j++) {
        /* Diagonal */
        new_move = index;
        do {
            new_move += delta_diagonal[j];
            if(!LEGAL_MOVE(new_move) || colours[new_move] == -1*by) break;
            if(pieces[new_move] != EMPTY && pieces[new_move] != BISHOP && pieces[new_move] != QUEEN) break;
            if(colours[new_move] == by && (pieces[new_move] == BISHOP || pieces[new_move] == QUEEN))
                return 1;
        } while(1);

        /* Vertical */
        new_move = index;
        do {
            new_move += delta_vertical[j];
            if(!LEGAL_MOVE(new_move) || colours[new_move] == -1*by) break;
            if(pieces[new_move] != EMPTY && pieces[new_move] != ROOK && pieces[new_move] != QUEEN) break;
            if(colours[new_move] == by && (pieces[new_move] == ROOK || pieces[new_move] == QUEEN))
                return 1;
        } while(1);
    }

    /* Knight */
    for(j = 0; j < 8; j++) {
        new_move = delta_knight[j] + index;
        if(LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == KNIGHT)
            return 1;
    }

    /* King */
    for(j = 0; j < 8; j++) {
        new_move = delta_king[j] + index;
        if(LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == KING)
            return 1;
    }

    return 0;
}

unsigned is_in_check(int side) {
    if(side == WHITE)
        return is_attacked(w_king, BLACK);
    else
        return is_attacked(b_king, WHITE);
}

void board_capture_piece(unsigned from, unsigned to) {
    switch(colours[to]) {
    case WHITE:
        zobrist ^= zobrist_w[pieces[to]][to];
        piece_list_subtract(&w_pieces, to);
        piece_list_subtract(&w_pieces_by_type[pieces[to]], to);
        break;
    case BLACK:
        zobrist ^= zobrist_b[pieces[to]][to];
        piece_list_subtract(&b_pieces, to);
        piece_list_subtract(&b_pieces_by_type[pieces[to]], to);
        break;
    }
    move_piece(from, to);
}

/* Used in subtract move, where zobrist isn't needed */
void move_piece_discreetly(unsigned from, unsigned to) {
    pieces[to] = pieces[from];
    colours[to] = colours[from];
    pieces[from] = EMPTY;
    colours[from] = EMPTY;
    switch(colours[to]) {
    case WHITE:
        piece_list_subtract(&w_pieces, from);
        piece_list_add(&w_pieces, to);

        piece_list_subtract(&w_pieces_by_type[pieces[to]], from);
        piece_list_add(&w_pieces_by_type[pieces[to]], to);
        break;
    case BLACK:
        piece_list_subtract(&b_pieces, from);
        piece_list_add(&b_pieces, to);

        piece_list_subtract(&b_pieces_by_type[pieces[to]], from);
        piece_list_add(&b_pieces_by_type[pieces[to]], to);
        break;
    }
}

void move_piece(unsigned from, unsigned to) {
    pieces[to] = pieces[from];
    colours[to] = colours[from];
    pieces[from] = EMPTY;
    colours[from] = EMPTY;

    switch(colours[to]) {
    case WHITE:
        piece_list_subtract(&w_pieces, from);
        piece_list_add(&w_pieces, to);

        piece_list_subtract(&w_pieces_by_type[pieces[to]], from);
        piece_list_add(&w_pieces_by_type[pieces[to]], to);

        zobrist ^= zobrist_w[pieces[to]][from];
        zobrist ^= zobrist_w[pieces[to]][to];
        break;
    case BLACK:
        piece_list_subtract(&b_pieces, from);
        piece_list_add(&b_pieces, to);

        piece_list_subtract(&b_pieces_by_type[pieces[to]], from);
        piece_list_add(&b_pieces_by_type[pieces[to]], to);

        zobrist ^= zobrist_b[pieces[to]][from];
        zobrist ^= zobrist_b[pieces[to]][to];
        break;
    }
}

/* Takes care of Zobrist key hashing */
void board_enpassant(unsigned new_enpassant_target) {
    if(enpassant_target != new_enpassant_target) {
        zobrist ^= zobrist_enpassant[enpassant_target];
        zobrist ^= zobrist_enpassant[new_enpassant_target];
        enpassant_target = new_enpassant_target;
    }
}

/* Generates a clean zobrist from a board */
ZOBRIST board_gen_zobrist() {
    ZOBRIST new_zobrist;
    unsigned i, index;
    new_zobrist = 0;

    for(i = 0; i < w_pieces.count; i++) {
        index = w_pieces.index[i];
        new_zobrist ^= zobrist_w[pieces[index]][index];
    }

    for(i = 0; i < b_pieces.count; i++) {
        index = b_pieces.index[i];
        new_zobrist ^= zobrist_b[pieces[index]][index];
    }

    new_zobrist ^= zobrist_enpassant[enpassant_target];
    if(castling & CASTLE_WK) new_zobrist ^= zobrist_castling[CASTLE_WK];
    if(castling & CASTLE_WQ) new_zobrist ^= zobrist_castling[CASTLE_WQ];
    if(castling & CASTLE_BK) new_zobrist ^= zobrist_castling[CASTLE_BK];
    if(castling & CASTLE_BQ) new_zobrist ^= zobrist_castling[CASTLE_BQ];
    if(turn == WHITE) new_zobrist ^= zobrist_side;

    return new_zobrist;
}
