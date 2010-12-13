#ifndef SEARCH_H 
#define SEARCH_H

uint64_t nodes_searched;

int qsearch(int, int);
int search(int, int, int);
unsigned search_root();
void moves_sort(unsigned* moves, unsigned move_count);

#endif
