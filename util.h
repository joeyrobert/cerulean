#ifndef UTIL_H
#define UTIL_H

unsigned piece_name_to_value(char);
char piece_value_to_name(unsigned);
unsigned piece_to_index(char*);
void index_to_piece(unsigned, char*);
void move_to_string(unsigned, char*);
void describe_moves(unsigned*, unsigned);

#endif
