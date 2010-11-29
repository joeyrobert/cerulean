#include <string.h>
#include "piece_list.h"
#include "board.h"

/*
  Piece list methods.

  Method to using a piece list:
    - They can be iterated over to yield the index of each piece.
    - Similar to mediocre's implementation, however abandons the
      use of global variables.

*/
void piece_list_new(piece_list* list) {
    memset(list, EMPTY, 145*sizeof(unsigned));
}

void piece_list_add(piece_list* list, unsigned index) {
    if(list->count < 16) {
        list->index[list->count] = index;
        list->reverse[index] = list->count;
        list->count += 1;
    }
}

void piece_list_subtract(piece_list* list, unsigned index) {
    unsigned loc;
    if(list->count > 0) {
        loc = list->reverse[index];
        list->count -= 1;
        list->index[loc] = list->index[list->count];
        list->reverse[list->index[loc]] = loc;
        list->index[list->count] = EMPTY;
        list->reverse[index] = EMPTY;
    }
}