#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "move.h"
#include "board.h"

unsigned piece_name_to_value(char name) {
      switch(name) {
          case 'p': return PAWN;
          case 'b': return BISHOP;
          case 'n': return KNIGHT;
          case 'r': return ROOK;
          case 'q': return QUEEN;
          case 'k': return KING;
          default:  return EMPTY;
      }
}

char piece_value_to_name(unsigned value) {
      switch(value) {
          case PAWN:   return 'p';
          case BISHOP: return 'b';
          case KNIGHT: return 'n';
          case ROOK:   return 'r';
          case QUEEN:  return 'q';
          case KING:   return 'k';
          case EMPTY:  return ' ';
          default:     return (char)value;
      }
}

unsigned piece_to_index(char* piece) {
    return ROWCOLUMN2INDEX((piece[1] - '0' - 1), (piece[0] - 'a'));
}

void index_to_piece(unsigned index, char* piece) {
    piece[0] = (char)('a' + INDEX2COLUMN(index));
    piece[1] = (char)(((int)'1') + INDEX2ROW(index));
    piece[2] = '\0';
}

void move_to_string(unsigned move, char* str) {
    char from[3], to[3];
    unsigned promote;

    index_to_piece(MOVE2TO(move), to);
    index_to_piece(MOVE2FROM(move), from);
    promote = MOVE2PROMOTE(move);

    str[0] = from[0];
    str[1] = from[1];
    str[2] = to[0];
    str[3] = to[1];
    if(promote != EMPTY) {
        str[4] = piece_value_to_name(promote);
        str[5] = '\0';
    } else
        str[4] = '\0';
}

/* Converts a move to short algebraic notation. */
void move_to_short_algebraic(unsigned move, char* str) {
    unsigned from, to, promote, loc, count, moves[256], i, check;
    unsigned other_from, other_to, add_row, add_column;
    char to_square[3], from_square[3];

    loc = 0;
    from = MOVE2FROM(move);
    to = MOVE2TO(move);
    promote = MOVE2PROMOTE(move);
    
    if(move & BITS_CASTLE) {
        if(to == 2 || to == 114)
            strcpy(str, "O-O-O");
        else            
            strcpy(str, "O-O");
        return;
    }

    /* Assumed knowledge */
    index_to_piece(to, to_square);
    index_to_piece(from, from_square);
    count = gen_moves(moves);
    add_row = 0;
    add_column = 0;

    /* Initial piece */
    if(pieces[from] != PAWN && pieces[from] != EMPTY)
        str[loc++] = toupper(piece_value_to_name(pieces[from]));

    /* Source square is ambigious */
    if(move & BITS_CAPTURE && pieces[from] == PAWN) {
        str[loc++] = from_square[0];
    } else {
        for(i = 0; i < count; i++) {
            /* Skip identical move */
            if(move == moves[i] || move & BITS_PROMOTE)
                continue;

            other_from = MOVE2FROM(moves[i]);
            other_to = MOVE2TO(moves[i]);
            
            /* Add file or rank information if other moves have the 
            same destination and piece type. */
            if(pieces[from] == pieces[other_from] && to == other_to) {
                if(INDEX2COLUMN(from) != INDEX2COLUMN(other_from))
                    add_column = 1;
                else if(INDEX2ROW(from) != INDEX2ROW(other_from))
                    add_row = 1;
                else {
                    add_row = 1;
                    add_column = 1;
                }
            }
        }
    }

    /* Add flags. This is so we can scale with 3 or more identical pieces
       attacking the same square */
    if(add_column) str[loc++] = from_square[0];
    if(add_row) str[loc++] = from_square[1];

    /* Capture squares */
    if(move & BITS_CAPTURE)
        str[loc++] = 'x';
    
    /* Destination square */
    str[loc++] = to_square[0];
    str[loc++] = to_square[1];

    /* Promotions */
    if(move & BITS_PROMOTE) {
        str[loc++] = '=';
        str[loc++] = toupper(piece_value_to_name(promote));
    }

    /* Board status (checkmate/stalemate) */
    board_add(move);

    count = gen_moves(moves); /* 2nd ply moves */
    check = is_in_check(turn);
    
    if(count == 0 && check)
        str[loc++] = '#';
    else if(count == 0)
        str[loc++] = '=';
    else if(check)
        str[loc++] = '+';
    
    board_subtract();

    str[loc++] = '\0';
}

void describe_moves(unsigned *moves, unsigned move_count) {
  unsigned i; char str[6];
  for(i = 0; i < move_count; i++) {
      move_to_string(moves[i], str);
      printf("%u %s\n", i, str);
  }
}

/* Returns a move if it exists. Otherwise results EMPTY */
unsigned find_move(char* move_string) {
    unsigned i, move_count, moves[256];
    char str[6];
    move_count = gen_moves(moves);

    for(i = 0; i < move_count; i++) {
        move_to_string(moves[i], str);
        if(strncmp(str, move_string, strlen(str)) == 0)
            return moves[i];
    }

    return EMPTY;
}

/* Returns a move if it exists. Otherwise results EMPTY */
unsigned find_short_algebraic_move(char* move_string) {
    unsigned i, count, moves[256];
    char str[10];
    count = gen_moves(moves);

    for(i = 0; i < count; i++) {
        move_to_short_algebraic(moves[i], str);

        if(strncmp(str, move_string, strlen(str)) == 0)
            return moves[i];
    }

    return EMPTY;
}
