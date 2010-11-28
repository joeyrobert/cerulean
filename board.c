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


int sort_compare(const void* a, const void* b) { return (*(unsigned*)a-*(unsigned*)b); }

void board_debug(char *image) {
    image[0] = '\0';
    int i, row, column;
    for(row = 7; row >= 0; row--) {
        for(column = 0; column < 8; column++) {
            char piece[1];
            piece[0] = piece_value_to_name(pieces[ROWCOLUMN2INDEX(row, column)]);
            if (colours[ROWCOLUMN2INDEX(row, column)] == WHITE)
                piece[0] = toupper(piece[0]);
            strncat(image, piece, 1);
        }
        strncat(image, "\n", 1000);
    }

    /* This part is NOT foolproof. Different board may appear identical. */
    unsigned black = 0, white = 0;
    for(i = 0; i < 16; i++) {
        white += w_pieces.index[i];
        black += b_pieces.index[i];
    }

    char buffer[50];
    sprintf(buffer, "W:%u B:%u", white, black);
    strncat(image, buffer, 1000);
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
    unsigned count = 0, i; piece_list* list;
    list = (turn == WHITE) ? &w_pieces : &b_pieces;

    for(i = 0; i < list->count; i++) {
        switch(pieces[list->index[i]]) {
            case PAWN:
               gen_pawn(&count, list->index[i], moves); 
               break;
            case KNIGHT:
               gen_knight(&count, list->index[i], moves); 
               break;
            case KING:
               gen_king(&count, list->index[i], moves); 
               break;
            case BISHOP:
               gen_bishop(&count, list->index[i], moves); 
               break;
            case ROOK:
               gen_rook(&count, list->index[i], moves); 
               break;
            case QUEEN:
               gen_bishop(&count, list->index[i], moves); 
               gen_rook(&count, list->index[i], moves);
               break;
        }
    }
    //gen_castle(&count, list->index[i], moves); 
    return count;
}

void gen_pawn(unsigned *count, unsigned i, unsigned* moves) {
    unsigned last_rank, first_rank, new_move;
    last_rank = (turn == WHITE) ? 7 : 0;
    first_rank = last_rank - 6 * turn;

    /* pawn push (w/ promotion) */
    new_move = i + turn*16;
    if((new_move & 0x88) == 0 && new_move < 120 && pieces[new_move] == EMPTY) {
        if(INDEX2ROW(new_move) == last_rank) {
            gen_promotions(count, moves, i, new_move, BITS_PAWN_MOVE);
        } else {
            moves[*count] = MOVE_NEW(BITS_PAWN_MOVE, 0, i, new_move);
            *count += 1; 
        }
    }

    /* double pawn push */
    new_move = i + turn*32;
    if((new_move & 0x88) == 0 && new_move < 120 && pieces[new_move] == EMPTY && pieces[i+turn*16] == EMPTY && INDEX2ROW(i) == first_rank) {
        moves[*count] = MOVE_NEW(BITS_PAWN_MOVE | BITS_PAWN_DOUBLE, 0, i, new_move);
        *count += 1;
    }

    int iter;
    for(iter = -1; iter <= 1; iter += 2) {
        /* captures */
        new_move = i + turn*16 + iter;
        if((new_move & 0x88) != 0 || new_move > 119) continue;

        if(colours[new_move] == -1 * turn) {
            if(INDEX2COLUMN(new_move) == last_rank) {
                gen_promotions(count, moves, i, new_move, BITS_PAWN_MOVE | BITS_CAPTURE);
            } else {
                moves[*count] = MOVE_NEW(BITS_PAWN_MOVE | BITS_CAPTURE, 0, i, new_move);
                *count += 1;
            }
        }

        /* en passant captures */
        if(en_passant_target == new_move) {
            moves[*count] = MOVE_NEW(BITS_EN_PASSANT | BITS_PAWN_MOVE | BITS_CAPTURE, 0, i, new_move);
            *count += 1;
        }
    }
}

void gen_knight(unsigned *count, unsigned i, unsigned* moves) {
    unsigned iter, new_move;
    for(iter = 0; iter < 8; iter++) {
        new_move = i + delta_knight[iter];

        if((new_move & 0x88) == 0 && new_move < 120 && colours[new_move] != turn) {
            if(colours[new_move] == -1*turn)
                moves[*count] = MOVE_NEW(BITS_CAPTURE, 0, i, new_move);
            else
                moves[*count] = MOVE_NEW(0, 0, i, new_move);
            *count += 1;
        }
    }
}

void gen_king(unsigned *count, unsigned i, unsigned* moves) {
    unsigned iter, new_move;
    for(iter = 0; iter < 8; iter++) {
        new_move = i + delta_king[iter];

        if((new_move & 0x88) == 0 && new_move < 120 && colours[new_move] != turn) {
            if(colours[new_move] == -1*turn)
                moves[*count] = MOVE_NEW(BITS_CAPTURE, 0, i, new_move);
            else
                moves[*count] = MOVE_NEW(0, 0, i, new_move);
            *count += 1;
        }
    }
}

void gen_bishop(unsigned *count, unsigned i, unsigned* moves) {
    unsigned iter; int new_move;
    for(iter = 0; iter < 4; iter++) {
        new_move = i + delta_diagonal[iter];

        while(1) {
            if((new_move & 0x88) == 0 && colours[new_move] == EMPTY) {
                moves[*count] = MOVE_NEW(0, 0, i, new_move); *count += 1;
            } else if((new_move & 0x88) == 0 && colours[new_move] == -1 * turn) {
                moves[*count] = MOVE_NEW(BITS_CAPTURE, 0, i, new_move); *count += 1;
                break;
            } else
                break;

            new_move += delta_diagonal[iter];
        }
    }
}

void gen_rook(unsigned *count, unsigned i, unsigned* moves) {
    unsigned iter; int new_move;
    for(iter = 0; iter < 4; iter++) {
        new_move = i + delta_diagonal[iter];

        while(1) {
            if((new_move & 0x88) == 0 && colours[new_move] == EMPTY) {
                moves[*count] = MOVE_NEW(0, 0, i, new_move); *count += 1;
            } else if((new_move & 0x88) == 0 && colours[new_move] == -1 * turn) {
                moves[*count] = MOVE_NEW(BITS_CAPTURE, 0, i, new_move); *count += 1;
                break;
            } else
                break;

            new_move += delta_diagonal[iter];
        }
    }
}

void gen_promotions(unsigned* count, unsigned* moves, unsigned from, unsigned to, unsigned bits) {
    moves[*count] = MOVE_NEW(bits | BITS_PROMOTE, QUEEN, from, to);  *count += 1;
    moves[*count] = MOVE_NEW(bits | BITS_PROMOTE, ROOK, from, to);   *count += 1;
    moves[*count] = MOVE_NEW(bits | BITS_PROMOTE, BISHOP, from, to); *count += 1;
    moves[*count] = MOVE_NEW(bits | BITS_PROMOTE, KNIGHT, from, to); *count += 1;
}

/* updates board and board list */
unsigned board_add(unsigned move) {
    /* save some important history */
    history[total_history][0] = move;
    history[total_history][1] = pieces[MOVE2TO(move)]; /* piece that existed there before,         */
    history[total_history][2] = en_passant_target;     /* will either be empty or -1 * turn colour */
    history[total_history][3] = castling;
    total_history++;

    piece_list *my_pieces, *their_pieces;
    unsigned to = MOVE2TO(move); unsigned from = MOVE2FROM(move); unsigned bits = MOVE2BITS(move);

    if(turn == WHITE) {
        my_pieces = &w_pieces;
        their_pieces = &b_pieces;
    } else {
        my_pieces = &b_pieces;
        their_pieces = &w_pieces;
    }

    if(bits == 0) {
        en_passant_target = NO_EN_PASSANT;
        move_piece(from, to, my_pieces);
    } else if ((bits & BITS_EN_PASSANT) != 0) {
        en_passant_target = NO_EN_PASSANT;
        piece_list_subtract(their_pieces, en_passant_target - turn*16);
        move_piece(from, to, my_pieces);
        pieces[en_passant_target - turn*16] = EMPTY;
        colours[en_passant_target - turn*16] = EMPTY;
    } else if ((bits & BITS_CAPTURE) != 0) {
        if(pieces[to] == KING) {
            switch(colours[to]) {
                case WHITE:
                    w_king = EMPTY;
                    break;
                case BLACK:
                    b_king = EMPTY;
                    break;
            }
        }
        en_passant_target = NO_EN_PASSANT;
        move_piece(from, to, my_pieces);
        piece_list_subtract(their_pieces, to);
    } else if ((bits & BITS_PAWN_DOUBLE) != 0) {
        en_passant_target = from + 16*turn;
        move_piece(from, to, my_pieces);
    } else if ((bits & BITS_PAWN_MOVE) != 0) {
        en_passant_target = NO_EN_PASSANT;
        move_piece(from, to, my_pieces);
    } else if ((bits & BITS_CASTLE) != 0) {
        /* move king */
        en_passant_target = NO_EN_PASSANT;
        move_piece(from, to, my_pieces);

        switch(to) {
            case 6:
                move_piece(7, 5, my_pieces);
                break;
            case 2:
                move_piece(0, 3, my_pieces);
                break;
            case 118:
                move_piece(119, 117, my_pieces);
                break;
            case 114:
                move_piece(112, 115, my_pieces);
                break;
        }
    }

    if ((bits & BITS_PROMOTE) != 0)
        pieces[to] = MOVE2PROMOTE(move);

    /* Post move */
    /* Remove castling rights and reset king */
    if(pieces[to] == KING) {
        switch(turn) {
            case WHITE:
                if(castling & CASTLE_WQ) castling -= CASTLE_WQ;
                if(castling & CASTLE_WK) castling -= CASTLE_WK;
                w_king = to;
                break;
            case BLACK:
                if(castling & CASTLE_BQ) castling -= CASTLE_BQ;
                if(castling & CASTLE_BK) castling -= CASTLE_BK;
                b_king = to;
                break;
        }
    }

    /* Remove castling rights */
    if((from == 7 || to == 7) && (castling & CASTLE_WK))     castling -= CASTLE_WK;
    if((from == 0 || to == 0) && (castling & CASTLE_WQ))     castling -= CASTLE_WQ;
    if((from == 119 || to == 119) && (castling & CASTLE_BK)) castling -= CASTLE_BK;
    if((from == 112 || to == 112) && (castling & CASTLE_BQ)) castling -= CASTLE_BQ;

    if(turn == BLACK) full_move_number++;
    turn = -1 * turn;

    if(is_in_check(-1 * turn)) {
        board_subtract();
        return 0;
    }

    return 1;
}

void board_subtract() {
    total_history--;
    turn = -1 * turn;
    if(turn == BLACK) full_move_number--;

    /* retreive some important history */
    unsigned move = history[total_history][0];
    castling = history[total_history][3];
    en_passant_target = history[total_history][2];

    piece_list *my_pieces, *their_pieces;
    unsigned to = MOVE2TO(move); unsigned from = MOVE2FROM(move); unsigned bits = MOVE2BITS(move);

    if(turn == WHITE) {
        my_pieces = &w_pieces;
        their_pieces = &b_pieces;
    } else {
        my_pieces = &b_pieces;
        their_pieces = &w_pieces;
    }

    if(bits == 0) {
        move_piece(to, from, my_pieces);
    } else if ((bits & BITS_EN_PASSANT) != 0) {
        move_piece(to, from, my_pieces);
        piece_list_add(their_pieces, en_passant_target - turn*16);
    } else if ((bits & BITS_CAPTURE) != 0) {
        move_piece(to, from, my_pieces);
        piece_list_add(their_pieces, to);
        pieces[to] = history[total_history][1];
        colours[to] = -1 * turn;

        if(pieces[to] == KING) {
            switch(colours[to]) {
                case WHITE:
                    w_king = to;
                    break;
                case BLACK:
                    b_king = to;
                    break;
            }
        }
    } else if ((bits & BITS_PAWN_DOUBLE) != 0) {
        move_piece(to, from, my_pieces);
    } else if ((bits & BITS_PAWN_MOVE) != 0) {
        move_piece(to, from, my_pieces);
    } else if ((bits & BITS_CASTLE) != 0) {
        move_piece(to, from, my_pieces);

        switch(to) {
            case 6:
                move_piece(5, 7, my_pieces);
                break;
            case 2:
                move_piece(3, 0, my_pieces);
                break;
            case 118:
                move_piece(117, 119, my_pieces);
                break;
            case 114:
                move_piece(115, 112, my_pieces);
                break;
        }
    }

    if((bits & BITS_PROMOTE) != 0)
        pieces[from] = PAWN;

    if(pieces[from] == KING) {
        if(colours[from] == WHITE)
            w_king = from;
        else
            b_king = from;
    }
}

void move_piece(unsigned from, unsigned to, piece_list* list) {
    list->reverse[to] = list->reverse[from];
    list->index[list->reverse[to]] = to;
    pieces[to] = pieces[from];
    colours[to] = colours[from];
    pieces[from] = EMPTY;
    colours[from] = EMPTY;
}

unsigned is_attacked(unsigned i, int by) {
    unsigned new_move, iter;

    /* Diagonal (bishop and queen) */
    for(iter = 0; iter < 4; iter++) {
        new_move = delta_diagonal[iter] + i;
        while(1) {
            if(!LEGAL_MOVE(new_move)) break;
            if(colours[new_move] == EMPTY)
                new_move += delta_diagonal[iter];
            else if((pieces[new_move] == BISHOP || pieces[new_move] == QUEEN) && colours[new_move] == by) {
 //               printf("FUCK DIAGONAL %u => %u\n", i, new_move);
                return 1;
            } else
                break;
        }
    }

    /* Horizontal and Vertical (rook and queen) */
    for(iter = 0; iter < 4; iter++) {
        new_move = delta_vertical[iter] + i;
        while(1) {
            if(!LEGAL_MOVE(new_move)) break;
            if(colours[new_move] == EMPTY)
                new_move += delta_vertical[iter];
            else if((pieces[new_move] == ROOK || pieces[new_move] == QUEEN) && colours[new_move] == by) {
       //         printf("FUCK VERTICAL %u => %u\n", i, new_move);
                return 1;
            } else
                break;
        }
    }

    /* Knight */
    for(iter = 0; iter < 8; iter++) {
        new_move = delta_knight[iter] + i;
        if(LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == KNIGHT) {
     //       printf("by: %i, colours[new_move]: %i, pieces[new_move]: %u\n", by, colours[new_move], pieces[new_move]);
     //       printf("by: %i, colours[i]: %i, pieces[i]: %u\n", by, colours[i], pieces[i]);
     //       printf("FUCK KNIGHT %u => %u\n", i, new_move);
     //       board_draw();
            return 1;
        }
    }

    /* King */
    for(iter = 0; iter < 8; iter++) {
        new_move = delta_king[iter] + i;
        if(LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == KING) {
   //         printf("FUCK KING %u => %u\n", i, new_move);
            return 1;
        }
    }

    /* Pawns */
    new_move = i + (15 * by) * -1;
    if (LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == PAWN) {
 //       printf("FUCK PAWN %u => %u\n", i, new_move);
        return 1;
    }

    new_move = i + (17 * by) * -1;
    if (LEGAL_MOVE(new_move) && colours[new_move] == by && pieces[new_move] == PAWN) {
    //    printf("FUCK PAWN %u => %u\n", i, new_move);
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
