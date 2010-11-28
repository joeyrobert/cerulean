#ifndef PIECE_LIST_H 
#define PIECE_LIST_H

/* size: 1 + 16 + 128 = 145 * sizeof(unsigned) */
typedef struct {
  unsigned count;        /* total number available */
  unsigned index[18];    /* piece => board index */
  unsigned reverse[128]; /* board_index => piece */
} piece_list;

void piece_list_add(piece_list*, unsigned);
void piece_list_subtract(piece_list*, unsigned);

#endif
