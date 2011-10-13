#ifndef XBOARD_H 
#define XBOARD_H

#include "tree.h"

int n_time, n_otim, out_of_opening;
tree_node* book_root_node, *book_current_node;
unsigned next_move();
void xboard_run();

#endif
