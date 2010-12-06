#ifndef SEARCH_H 
#define SEARCH_H

unsigned pv[64];
unsigned pv_depth;
uint64_t nodes_searched;

int quiesc_search(int, int);
int alphabeta_search(int, int, int);
unsigned think(int);

#endif
