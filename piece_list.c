#include "piece_list.h"

/*
  Piece list methods. See Mediocre for inspiration.

  Method to using a piece list:
    - They can be iterated over to yield the index of each piece.
    - Similar to mediocre's implementation, however abandons the
      use of global variables.

*/

void piece_list_add(piece_list* list, unsigned index) {
    list->reverse[index] = list->count;
    list->index[list->count] = index;
    list->count++;
}

void piece_list_subtract(piece_list* list, unsigned index) {
    list->count--;
    unsigned list_index = list->reverse[index];
    list->index[list_index] = list->index[list->count];     /* shift the last element into */
    list->reverse[list->index[list->count]] = list_index;   /* the available slot */
}
