#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include "search.h"
#include "board.h"
#include "evaluate.h"
#include "util.h"

int quiesc_search(int alpha, int beta) {
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
    
    // TODO: Put captures at the top of the list + additional move ordering.
    count = gen_moves(moves);

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
    int i, j, score;
    clock_t start, end;
    double elapsed_centiseconds;
    char move_strings[384], move_string[6];
    pv_depth = 0;
    nodes_searched = 0;

    start = clock();
    for(i = 1; ; i++) {
        memset(pv, 0, 64);
        memset(move_strings, '\0', 384);
        score = alphabeta_search(i, -INFINITE, INFINITE);
        end = clock();
        
        for(j = 0; ; j++) {
            if(pv[j] == 0) break;
            move_to_string(pv[j], move_string);
            strcat(move_strings, move_string);
            strcat(move_strings, " ");
        }

        end = clock();
        elapsed_centiseconds = (double)(end - start) / (CLOCKS_PER_SEC * 100);
        printf("%i %i %.5f %llu %s\n", i, score, elapsed_centiseconds, nodes_searched, move_strings);

        if (elapsed_centiseconds > 0.1 * total_centiseconds)        break;
        if (score >= INFINITE - 1000 || score <= -INFINITE + 1000)  break;
    }

    return pv[0];
}

