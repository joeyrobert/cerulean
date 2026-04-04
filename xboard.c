/* xboard.c - XBoard/CECP protocol interface
 *
 * Matches ceruleanjs command set including:
 *   protover, memory, sd, st, level, book, moves, evaluate,
 *   cachestat, version, remove, force, usermove, ping/pong,
 *   sts, option, time management
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include "xboard.h"
#include "board.h"
#include "perft.h"
#include "test.h"
#include "util.h"
#include "zobrist.h"
#include "search.h"
#include "evaluate.h"
#include "nnue.h"
#include "sts.h"

#define VERSION "2.0.0"

static int force_mode = 0;
static int game_over = 0;
static unsigned move_history[2048];
static int move_history_count = 0;
static int use_book = 0;  /* opening book not loaded by default */

/* -----------------------------------------------------------------------
 * Result detection
 * ----------------------------------------------------------------------- */
static int check_result(int hide) {
    unsigned moves[256];
    unsigned count = 0, i;
    int in_check;

    /* Count legal moves */
    count = gen_moves(moves);
    int legal = 0;
    for (i = 0; i < count; i++) {
        if (board_add(moves[i])) {
            legal = 1;
            board_subtract();
            break;
        }
    }

    if (!legal) {
        in_check = is_in_check(turn);
        if (in_check) {
            if (turn == WHITE) {
                if (!hide) printf("0-1 {Black mates}\n");
            } else {
                if (!hide) printf("1-0 {White mates}\n");
            }
        } else {
            if (!hide) printf("1/2-1/2 {Stalemate}\n");
        }
        game_over = 1;
        return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Engine move
 * ----------------------------------------------------------------------- */
static void engine_go(void) {
    char move_str[10];
    unsigned move;

    if (game_over) return;
    force_mode = 0;

    update_time_per_move();
    move = iterative_deepening((int)time_per_move_ms, max_depth, 0);

    if (!move) {
        /* No legal moves */
        check_result(0);
        return;
    }

    if (!board_add(move)) {
        printf("Error (engine produced illegal move)\n");
        return;
    }

    move_history[move_history_count++] = move;
    move_to_string(move, move_str);
    printf("move %s\n", move_str);
    fflush(stdout);

    /* Track time: subtract time used (approximate) */
    engine_time_ms -= (int)(get_time_ms() - search_start_ms);
    if (engine_time_ms < 0) engine_time_ms = 0;
    engine_time_ms += increment_ms;

    check_result(0);
}

/* -----------------------------------------------------------------------
 * Command handlers
 * ----------------------------------------------------------------------- */

static void cmd_display(void)        { board_draw(); }
static void cmd_quit(void)           { printf("Goodbye.\n"); exit(0); }
static void cmd_force(void)          { force_mode = 1; }
static void cmd_xboard(void)         { printf("\n"); }
static void cmd_random(void)         {}
static void cmd_post(void)           {}
static void cmd_hard(void)           {}
static void cmd_easy(void)           {}
static void cmd_nps(void)            {}
static void cmd_accepted(void)       {}

static void cmd_new(void) {
    force_mode = 0;
    game_over = 0;
    move_history_count = 0;
    max_depth = 64;
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

static void cmd_go(void) {
    force_mode = 0;
    engine_go();
}

static void cmd_undo(void) {
    game_over = 0;
    if (move_history_count > 0) {
        board_subtract();
        move_history_count--;
    }
}

static void cmd_remove(void) {
    cmd_undo();
    cmd_undo();
}

static void cmd_setboard(const char *fen) {
    board_set_fen((char*)fen);
    game_over = 0;
}

static void cmd_white(void) { turn = WHITE; }
static void cmd_black(void) { turn = BLACK; }

static void cmd_time(const char *arg) {
    engine_time_ms = atoi(arg) * 10;  /* centiseconds -> ms */
    update_time_per_move();
}

static void cmd_otim(const char *arg) {
    /* opponent time, just store it */
    n_otim = atoi(arg);
    (void)n_otim;
}

static void cmd_st(const char *arg) {
    time_per_move_ms = (long)(atof(arg) * 1000.0);
}

static void cmd_sd(const char *arg) {
    max_depth = atoi(arg);
    if (max_depth < 1) max_depth = 1;
    if (max_depth >= MAX_PLY) max_depth = MAX_PLY - 1;
}

static void cmd_level(const char *arg) {
    /* level MPT BASE INC  (BASE can be mm:ss or mm) */
    int mpt, inc;
    char base_str[32];
    if (sscanf(arg, "%d %31s %d", &mpt, base_str, &inc) < 2) {
        printf("Error (Invalid level): level %s\n", arg);
        return;
    }
    /* Parse base: mm or mm:ss */
    int base_min = 0, base_sec = 0;
    char *colon = strchr(base_str, ':');
    if (colon) {
        base_min = atoi(base_str);
        base_sec = atoi(colon + 1);
    } else {
        base_min = atoi(base_str);
    }
    moves_per_tc  = mpt;
    base_time_ms  = (base_min * 60 + base_sec) * 1000;
    increment_ms  = inc * 1000;
    engine_time_ms = base_time_ms;
    update_time_per_move();
}

static void cmd_memory(const char *arg) {
    int mb = atoi(arg);
    if (mb < 1) mb = 1;

    /* Search table: 50% of memory */
    int search_mb = mb / 2;
    int exp = 0;
    uint64_t sz = (uint64_t)sizeof(hash_node);
    while ((sz << (exp+1)) <= (uint64_t)(search_mb * 1024 * 1024))
        exp++;
    free(table->table);
    table->size = (uint64_t)1 << exp;
    table->table = (hash_node*)calloc(table->size, sizeof(hash_node));

    /* Eval and pawn tables: 25% each */
    init_eval_tables(mb / 2);
    printf("Memory set to %dMB\n", mb);
}

static void cmd_perfthash(const char *arg) {
    int exp = atoi(arg);
    /* Resize search table as perft hash */
    if (exp <= 0) {
        printf("Perft hash table removed\n");
    } else {
        free(table->table);
        table->size = (uint64_t)1 << exp;
        table->table = (hash_node*)calloc(table->size, sizeof(hash_node));
        printf("Perft hash size set to 2^%d = %llu\n", exp,
               (unsigned long long)table->size);
    }
}

static void cmd_perft(const char *arg) {
    int depth = atoi(arg);
    if (depth < 1) { printf("Error (perft depth not provided)\n"); return; }

    clock_t start = clock();
    uint64_t total = perft_perft(depth);
    double t = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("%llu\ntime %.0f ms\nfreq %.0f Hz\n",
           (unsigned long long)total,
           t * 1000.0,
           total / (t > 0 ? t : 1e-9));
}

static void cmd_divide(const char *arg) {
    int depth = atoi(arg);
    if (depth < 1) { printf("Error (divide depth not provided)\n"); return; }
    perft_divide(depth);
}

static void cmd_moves(void) {
    unsigned moves[256], count, i;
    char str[10];
    count = gen_moves(moves);
    for (i = 0; i < count; i++) {
        if (board_add(moves[i])) {
            board_subtract();
            move_to_short_algebraic(moves[i], str);
            printf("%s\n", str);
        }
    }
}

static void cmd_evaluate(void) {
    hce_evaluation(1);
    if (nnue_can_evaluate())
        printf("NNUE.........%5d (side-to-move perspective)\n", nnue_evaluate());
}

static void cmd_nnue(const char *arg) {
    if (!arg || !arg[0] || strcmp(arg, "status") == 0 || strcmp(arg, "info") == 0) {
        printf("NNUE loaded:  %s\n", nnue_is_loaded() ? "yes" : "no");
        printf("NNUE enabled: %s\n", nnue_is_enabled() ? "yes" : "no");
        printf("NNUE path:    %s\n", nnue_loaded_path() ? nnue_loaded_path() : "(none)");
        return;
    }

    if (strcmp(arg, "off") == 0) {
        nnue_set_enabled(0);
        printf("NNUE disabled\n");
        return;
    }

    if (strcmp(arg, "on") == 0) {
        if (!nnue_is_loaded()) {
            printf("NNUE not loaded\n");
            return;
        }
        nnue_set_enabled(1);
        nnue_refresh();
        printf("NNUE enabled\n");
        return;
    }

    if (strcmp(arg, "unload") == 0) {
        nnue_unload();
        printf("NNUE unloaded\n");
        return;
    }

    if (strncmp(arg, "load ", 5) == 0) {
        if (nnue_load(arg + 5))
            printf("NNUE loaded: %s\n", arg + 5);
        else
            printf("Error (failed to load NNUE): %s\n", arg + 5);
        return;
    }

    printf("Error (unknown nnue command): %s\n", arg);
}

static void cmd_book(const char *arg) {
    /* Opening book not loaded; report status */
    char status[32];
    sscanf(arg, "%31s", status);
    if (!use_book && status[0] != '\0') {
        printf("Book not loaded, remains off\n");
    } else {
        printf("Book not loaded, remains off\n");
    }
}

static void cmd_cachestat(void) {
    printf("SEARCH: Size: %llu entries\n", (unsigned long long)table->size);
    if (eval_table_size)
        printf("EVAL:   Size: %llu entries\n", (unsigned long long)eval_table_size);
    if (pawn_table_size)
        printf("PAWN:   Size: %llu entries\n", (unsigned long long)pawn_table_size);
}

static void cmd_version(void) {
    printf("CeruleanC %s by Joey Robert\n", VERSION);
}

static void cmd_sts(const char *arg) {
    int time_ms = 1000;
    if (arg && arg[0]) time_ms = (int)(atof(arg) * 1000.0);
    sts_run(time_ms);
}

static void cmd_ping(const char *arg) {
    printf("pong %s\n", arg);
    fflush(stdout);
}

static void cmd_option(const char *arg) {
    /* option name=value: adjust eval parameters at runtime */
    /* Parameters are compile-time constants; log receipt */
    printf("# option %s (runtime tuning not yet supported)\n", arg);
}

static void cmd_protover(void) {
    printf("feature myname=\"CeruleanC %s by Joey Robert\"\n", VERSION);
    printf("feature setboard=1\n");
    printf("feature memory=1\n");
    printf("feature time=1\n");
    printf("feature usermove=1\n");
    printf("feature done=1\n");
    fflush(stdout);
}

static void cmd_perfttest(void) { perft_test(); }
static void cmd_searchtest(void) { search_test(); }

static void cmd_help(void) {
    printf("\nCeruleanC %s, C Chess Engine by Joey Robert\n\n", VERSION);
    printf("Command                     Description\n\n");
    printf("display                     Draws the board\n");
    printf("perft [INT]                 Perfts the current board to specified depth\n");
    printf("perfthash [INT]             Sets perft hashtable exponent (size 2^exponent)\n");
    printf("memory [INT]                Sets the memory used by the engine in megabytes\n");
    printf("divide [INT]                Divides the current board to specified depth\n");
    printf("moves                       Lists valid moves for this position\n");
    printf("e2e4                        Moves from the current position and thinks\n");
    printf("go                          Forces the engine to think\n");
    printf("undo                        Subtracts the previous move\n");
    printf("remove                      Subtracts the previous two moves\n");
    printf("new                         Sets up the default board position\n");
    printf("setboard [FEN]              Sets the board using Forsyth-Edwards Notation\n");
    printf("evaluate                    Performs a static evaluation of the board\n");
    printf("book [on|off]               Toggles whether engine uses opening book\n");
    printf("white                       Sets the active colour to WHITE\n");
    printf("black                       Sets the active colour to BLACK\n");
    printf("time [INT]                  Sets engine's time (in centiseconds)\n");
    printf("otim [INT]                  Sets opponent's time (in centiseconds)\n");
    printf("sd [INT]                    Sets maximum depth\n");
    printf("st [INT]                    Sets maximum time (seconds)\n");
    printf("level [MPT] [BASE] [INC]    Sets Winboard level timing\n");
    printf("sts [TIME_S]                Run Strategic Test Suite (default 1s per move)\n");
    printf("version                     Outputs the version number\n");
    printf("perfttest                   Runs the perft test suite\n");
    printf("searchtest                  Runs the search test suite\n");
    printf("cachestat                   Prints cache statistics\n");
    printf("nnue [status|on|off|load]   Controls NNUE evaluation\n");
    printf("exit                        Exits the engine\n");
    printf("quit                        See exit\n");
    printf("help                        Gets you this magical menu\n\n");
}

/* -----------------------------------------------------------------------
 * Move regex: matches e2e4, e7e8q style
 * ----------------------------------------------------------------------- */
static int is_move_string(const char *s) {
    /* from-file from-rank to-file to-rank [promo] */
    if (strlen(s) < 4) return 0;
    if (s[0] < 'a' || s[0] > 'h') return 0;
    if (s[1] < '1' || s[1] > '8') return 0;
    if (s[2] < 'a' || s[2] > 'h') return 0;
    if (s[3] < '1' || s[3] > '8') return 0;
    return 1;
}

static void handle_usermove(const char *move_string) {
    char str[10];
    unsigned move;

    if (game_over) return;

    move = find_move((char*)move_string);
    if (move == EMPTY) {
        printf("Illegal move: %s\n", move_string);
        return;
    }

    if (!board_add(move)) {
        printf("Illegal move: %s\n", move_string);
        return;
    }

    move_history[move_history_count++] = move;
    (void)str;

    if (check_result(0)) return;
    if (!force_mode) engine_go();
}

/* -----------------------------------------------------------------------
 * Main command loop
 * ----------------------------------------------------------------------- */
void xboard_run(void) {
    char command[2000];
    setbuf(stdout, NULL);

    /* Initialize */
    n_time = 0; n_otim = 0;
    moves_per_tc   = 40;
    base_time_ms   = 4 * 60 * 1000;   /* 4 minutes */
    increment_ms   = 0;
    engine_time_ms = base_time_ms;
    max_depth      = 64;

    zobrist_fill();
    board_create_table();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    generate_PST();
    cmd_memory("100");  /* default 100MB */
    nnue_load("nets/default.nnue");
    update_time_per_move();

    while (fgets(command, sizeof(command), stdin) != NULL) {
        /* Strip trailing newline/whitespace */
        int len = (int)strlen(command);
        while (len > 0 && (command[len-1] == '\n' || command[len-1] == '\r' ||
                           command[len-1] == ' '))
            command[--len] = '\0';

        if (len == 0) continue;

        /* Split command into verb + argument */
        char verb[64] = {0};
        char arg[1024] = {0};
        sscanf(command, "%63s", verb);
        if (strlen(command) > strlen(verb) + 1)
            strncpy(arg, command + strlen(verb) + 1, sizeof(arg) - 1);

        /* Move? */
        if (is_move_string(verb)) {
            handle_usermove(verb);
        } else if (strcmp(verb, "usermove") == 0) {
            handle_usermove(arg);
        } else if (strcmp(verb, "xboard") == 0)   { cmd_xboard(); }
        else if (strcmp(verb, "protover") == 0)    { cmd_protover(); }
        else if (strcmp(verb, "new") == 0)         { cmd_new(); }
        else if (strcmp(verb, "go") == 0)          { cmd_go(); }
        else if (strcmp(verb, "force") == 0)       { cmd_force(); }
        else if (strcmp(verb, "undo") == 0)        { cmd_undo(); }
        else if (strcmp(verb, "remove") == 0)      { cmd_remove(); }
        else if (strcmp(verb, "setboard") == 0)    { cmd_setboard(arg); }
        else if (strcmp(verb, "white") == 0)       { cmd_white(); }
        else if (strcmp(verb, "black") == 0)       { cmd_black(); }
        else if (strcmp(verb, "time") == 0)        { cmd_time(arg); }
        else if (strcmp(verb, "otim") == 0)        { cmd_otim(arg); }
        else if (strcmp(verb, "st") == 0)          { cmd_st(arg); }
        else if (strcmp(verb, "sd") == 0)          { cmd_sd(arg); }
        else if (strcmp(verb, "level") == 0)       { cmd_level(arg); }
        else if (strcmp(verb, "memory") == 0)      { cmd_memory(arg); }
        else if (strcmp(verb, "perft") == 0)       { cmd_perft(arg); }
        else if (strcmp(verb, "perfthash") == 0)   { cmd_perfthash(arg); }
        else if (strcmp(verb, "divide") == 0)      { cmd_divide(arg); }
        else if (strcmp(verb, "moves") == 0)       { cmd_moves(); }
        else if (strcmp(verb, "display") == 0 ||
                 strcmp(verb, "draw") == 0)        { cmd_display(); }
        else if (strcmp(verb, "evaluate") == 0 ||
                 strcmp(verb, "eval") == 0)        { cmd_evaluate(); }
        else if (strcmp(verb, "book") == 0)        { cmd_book(arg); }
        else if (strcmp(verb, "cachestat") == 0)   { cmd_cachestat(); }
        else if (strcmp(verb, "nnue") == 0)        { cmd_nnue(arg); }
        else if (strcmp(verb, "version") == 0)     { cmd_version(); }
        else if (strcmp(verb, "sts") == 0)         { cmd_sts(arg); }
        else if (strcmp(verb, "ping") == 0)        { cmd_ping(arg); }
        else if (strcmp(verb, "option") == 0)      { cmd_option(arg); }
        else if (strcmp(verb, "perfttest") == 0)   { cmd_perfttest(); }
        else if (strcmp(verb, "searchtest") == 0)  { cmd_searchtest(); }
        else if (strcmp(verb, "result") == 0)      { check_result(0); }
        else if (strcmp(verb, "?") == 0)           { printf("\n"); }
        else if (strcmp(verb, "quit") == 0 ||
                 strcmp(verb, "exit") == 0)        { cmd_quit(); }
        else if (strcmp(verb, "help") == 0)        { cmd_help(); }
        else if (strcmp(verb, "random") == 0)      { cmd_random(); }
        else if (strcmp(verb, "post") == 0)        { cmd_post(); }
        else if (strcmp(verb, "hard") == 0)        { cmd_hard(); }
        else if (strcmp(verb, "easy") == 0)        { cmd_easy(); }
        else if (strcmp(verb, "nps") == 0)         { cmd_nps(); }
        else if (strcmp(verb, "accepted") == 0)    { cmd_accepted(); }
        else if (strcmp(verb, "rejected") == 0)    {}
        else if (strcmp(verb, "computer") == 0)    {}
        else if (strcmp(verb, "variant") == 0)     {}
        else if (strcmp(verb, "analyze") == 0)     {}
        else if (strcmp(verb, "exit") == 0)        { cmd_quit(); }
        else {
            printf("Error (unknown command): %s\n", command);
            fflush(stdout);
        }
    }
}
