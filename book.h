#ifndef BOOK_H 
#define BOOK_H

unsigned moves[3543][20];
unsigned move_order[3543]; /* beginning of line */
unsigned opening_no;
int compare(const void* pm1, const void* pm2);
void generate_opening_book();
unsigned next_opening_move();
#endif
