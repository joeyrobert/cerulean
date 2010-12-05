#include <stdio.h>
#include <string.h>
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
