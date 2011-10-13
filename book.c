#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "book.h"
#include "board.h"
#include "util.h"
#include "tree.h"

tree_node* generate_opening_book() {
    char line[1000];
    unsigned move;
    char* move_string;
    FILE* file;
    int line_idx, line_len, moves_added, i, find_child_move;
    tree_node *root_node, *current_node, *child_node;

    file = fopen("BIGBOOK", "r");
    root_node = tree_new(EMPTY);

    /* Read in seed opening book into moves */
    while(fgets(line, 1000, file) != NULL) {
        line_idx = 0;
        moves_added = 0;
        line_len = strlen(line);
        current_node = root_node;

        while(line_idx < line_len-1) {
            move_string = substring(line, line_idx, 4);
            move = find_move(move_string);
            board_add(move);
            line_idx += 4;
            ++moves_added;
            find_child_move = 0;

            /* add node to below current, reset current */
            if(current_node->children_size > EMPTY) {
                /* look for current move in chrilden */
                child_node = current_node->down;
                for(i = 0; i < current_node->children_size; ++i) {
                    if(child_node->move == move) {
                        current_node = child_node;
                        find_child_move = 1;
                        break;
                    }
                    child_node = child_node->next; 
                }

                if(!find_child_move) {
                    /* did not find move in chrilden */
                    child_node->next = tree_new(move);
                    current_node = child_node->next;
                }
            } else {
                /* set down to current move if empty */
                current_node->down = tree_new(move);
                current_node = current_node->down;
            }
        }

        for(i = 0; i < moves_added; ++i) {
            board_subtract();
        }
    }

    return root_node;
}
