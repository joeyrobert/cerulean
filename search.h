#ifndef SEARCH_H
#define SEARCH_H

#include <stdint.h>
#include "evaluate.h"

#define MAX_PLY 64

/* Search statistics */
uint64_t nodes_searched;

/* PV table */
unsigned pv_table[MAX_PLY][MAX_PLY];
int pv_length[MAX_PLY];

/* Killer moves: [ply][0..1] */
unsigned killer_moves[MAX_PLY][2];

/* History heuristic: [turn_idx][from64 * 64 + to64]
   turn_idx: 0=white, 1=black */
#define HIST_SIZE (64 * 64)
uint32_t history_heuristic[2][HIST_SIZE];
uint32_t butterfly_heuristic[2][HIST_SIZE];

/* Time management */
long search_start_ms;
long time_per_move_ms;
int ended_early;

/* n_time / n_otim in centiseconds (from XBoard time command) */
int n_time;
int n_otim;

/* Time control */
int moves_per_tc;
int base_time_ms;
int increment_ms;
int engine_time_ms;
int max_depth;

long get_time_ms(void);
int qsearch(int alpha, int beta, int ply);
int search(int depth, int alpha, int beta, int ply, unsigned excluded_move);
unsigned search_root(void);
unsigned iterative_deepening(int time_ms, int max_dep, int hide_display);
void moves_sort(unsigned *moves, int *scores, unsigned move_count);
void update_time_per_move(void);
void clear_heuristics(void);

#endif
