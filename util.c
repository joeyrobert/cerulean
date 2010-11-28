#include <stdio.h>
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
    return ROWCOLUMN2INDEX((piece[1] - '0'), (piece[0] - 'a'));
}

void index_to_piece(unsigned index, char* piece) {
    piece[0] = (char)('a' + INDEX2COLUMN(index));
    piece[1] = (char)(((int)'0') + INDEX2ROW(index) + 1);
    piece[2] = '\0';
}

void move_to_string(unsigned move, char* str) {
    char from[3], to[3];
    index_to_piece(MOVE2TO(move), to);
    index_to_piece(MOVE2FROM(move), from);
    str[0] = from[0];
    str[1] = from[1];
    str[2] = to[0];
    str[3] = to[1];
    str[4] = '\0';
}

void describe_moves(unsigned *moves, unsigned move_count) {
  unsigned i; char str[5];
  for(i = 0; i < move_count; i++) {
      move_to_string(moves[i], str);
      printf("%s, ", str);
  }  
  printf("\n");
}
