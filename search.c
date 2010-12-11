#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include "search.h"
#include "board.h"
#include "evaluate.h"
#include "util.h"
#include "move.h"

int quiesc_search(int alpha, int beta) {
    unsigned moves[256], count, i;
    int stand_pat, score;

    stand_pat = turn*static_evaluation();

    if (stand_pat >= beta)
        return beta;
    if (alpha < stand_pat)
        alpha = stand_pat;

    count = gen_caps(moves);
    for(i = 0; i < count; i++) {
        if (!board_add(moves[i])) continue;
        score = -quiesc_search(-beta, -alpha);
        board_subtract();

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

int alphabeta_search(int depth, int alpha, int beta) {
    unsigned moves[256], count, i;
    int score;
    //hash_node* node;

    //node = hash_find(table, zobrist);
    //if(node != NULL && node->depth >= depth) {
    //    if (node->type == HASH_EXACT ||
    //        node->type == HASH_ALPHA && node->score <= alpha ||
    //        node->type == HASH_BETA  && node->score >= beta)
    //        return node->score;
    //}

    // Check for end of search or terminal node
    if (depth == 0 || pieces[w_king] == EMPTY || pieces[b_king] == EMPTY) {
        nodes_searched++;
        score = quiesc_search(alpha, beta);
        return score;
    }
    
    // TODO: Additional move ordering;
    count = gen_moves(moves);
    moves_sort(moves, count);

    for(i = 0; i < count; i++) {
        if (!board_add(moves[i])) continue;
        pv_depth++;
        score = -alphabeta_search(depth - 1, -beta, -alpha);
        pv_depth--;
        board_subtract();

        if (score > alpha) {
            alpha = score;
            pv[pv_depth] = moves[i];
        }

        if (beta <= alpha) {
            break;
        }
    }

    return alpha;
}

unsigned think(int total_centiseconds) {
    int depth, j, score;
    clock_t start, end;
    double elapsed_centiseconds;
    char move_strings[384], move_string[6];
    pv_depth = 0;
    nodes_searched = 0;
    
    depth = 4;

    start = clock();
    memset(pv, 0, 64);
    memset(move_strings, '\0', 384);
    score = alphabeta_search(depth, -INFINITE, INFINITE);
    end = clock();
        
    for(j = 0; ; j++) {
        if(pv[j] == 0) break;
        move_to_string(pv[j], move_string);
        strcat(move_strings, move_string);
        strcat(move_strings, " ");
    }

    elapsed_centiseconds = (double)(end - start) / (CLOCKS_PER_SEC * 100);
    printf("%i %i %.5f %llu %s\n", depth, score, elapsed_centiseconds, nodes_searched, move_strings);

    return pv[0];
}

/* at the moment, this should place captures on the top */
void moves_sort(unsigned* moves, unsigned move_count) {
    unsigned beginning, tmp, i;
    beginning = 0;

    for(i = 0; i < move_count; i++) {
        if(moves[i] & BITS_CAPTURE || moves[i] & BITS_PROMOTE || moves[i] & BITS_CASTLE) {
            tmp = moves[beginning];
            moves[beginning] = moves[i];
            moves[i] = tmp;
            beginning++;
        }
    }
}
