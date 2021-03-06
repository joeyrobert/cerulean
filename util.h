#ifndef UTIL_H
#define UTIL_H

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)
#define ABS(a)      (((a) < 0) ? -1*(a) : (a))

unsigned piece_name_to_value(char);
char piece_value_to_name(unsigned);
unsigned piece_to_index(char*);
void index_to_piece(unsigned, char*);
void move_to_string(unsigned, char*);
void move_to_short_algebraic(unsigned, char*);
void describe_moves(unsigned*, unsigned);
unsigned find_move(char*);
unsigned find_short_algebraic_move(char*);
char* substring(char *string, int position, int length);

#endif
