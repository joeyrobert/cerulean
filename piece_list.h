#ifndef PIECE_LIST_H 
#define PIECE_LIST_H

/* Set to hold a max of 16 pieces, although 10 pieces is
   enough for any one piece type */
typedef struct {
    unsigned count;        /* total number available */
    unsigned index[16];    /* piece => board index */
    unsigned reverse[128]; /* board_index => piece */
} piece_list;

void piece_list_new(piece_list*);
void piece_list_add(piece_list*, unsigned);
void piece_list_subtract(piece_list*, unsigned);

#endif
