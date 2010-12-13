#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "search.h"
#include "board.h"
#include "evaluate.h"
#include "util.h"
#include "move.h"

int qsearch(int alpha, int beta) {
    unsigned moves[256], count, i;
    int stand_pat, score;

    stand_pat = static_evaluation();

    if (stand_pat >= beta)
        return beta;
    if (alpha < stand_pat)
        alpha = stand_pat;

    count = gen_caps(moves);
    for(i = 0; i < count; i++) {
        if (!board_add(moves[i])) continue;
        score = -qsearch(-beta, -alpha);
        board_subtract();

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

int search(int depth, int alpha, int beta) {
    unsigned moves[256], count, i, check;
    int score;
    hash_node *node;

    /* Check for end of search */
    if (depth == 0) {
        nodes_searched++;
        return qsearch(alpha, beta);
    }

    /* Check the Transposition table */
    node = hash_find(table, zobrist);
    if(node != NULL && node->depth >= depth) {
        switch(node->type) {
        case HASH_ALPHA:
            alpha = MAX(alpha, node->score);
            break;
        case HASH_BETA:
            beta = MIN(alpha, node->score);
            break;
        case HASH_EXACT:
            return node->score;
        }
    }
    
    /* Move ordering */
    count = gen_moves(moves);
    moves_sort(moves, count);

    /* Check extension */
    check = is_in_check(turn);
    if(check)
        depth++;

    for(i = 0; i < count; i++) {
        if (!board_add(moves[i])) continue;
        score = -search(depth - 1, -beta, -alpha);
        board_subtract();

        if(score >= beta) {
            /* beta cutoff */
            return beta;
        }

        if(score > alpha) {
            /* new best score */
            alpha = score;
        }

    }

    /* checkmate or stalemate */
    if(count == 0) {
        if(check)
            return -INFINITE;
        return 0;
    }

    return alpha;
}

unsigned search_root() {
    int depth, i, score, best_score, count;
    unsigned moves[256], best_move;
    nodes_searched = 0;
    
    /* Depth is fixed for now */
    depth = 4;
    
    /* Move ordering */
    count = gen_moves(moves);
    moves_sort(moves, count);

    best_score = -INFINITE;
    best_move = 0;
    
    for(i = 0; i < count; i++) {
        if (!board_add(moves[i])) continue;
        score = -search(depth - 1, -INFINITE, INFINITE);
        board_subtract();

        /* sometimes king related moves don't
        override the check at the bottom */
        if(best_move == 0)
            best_move = moves[i];

        if(score > best_score) {
            best_move = moves[i];
            best_score = score;
        }
    }

    return best_move;
}

/* at the moment, this should place captures on the top */
void moves_sort(unsigned* moves, unsigned move_count) {
    unsigned beginning, tmp, i;
    beginning = 0;

    for(i = 0; i < move_count; i++) {
        if(moves[i] & BITS_CAPTURE || moves[i] & BITS_PROMOTE) {
            tmp = moves[beginning];
            moves[beginning] = moves[i];
            moves[i] = tmp;
            beginning++;
        }
    }
}
