#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include "cli_tools.h"
#include "board.h"
#include "evaluate.h"
#include "hash_table.h"
#include "move.h"
#include "nnue.h"
#include "search.h"
#include "sts.h"
#include "util.h"
#include "zobrist.h"

#define TRAINING_RESULT_BLACK 0
#define TRAINING_RESULT_DRAW  1
#define TRAINING_RESULT_WHITE 2
#define TRAINING_RESULT_UNKNOWN 3

typedef struct {
    uint8_t pieces[32];
    uint8_t stm;
    uint8_t castling;
    uint8_t ep_square;
    uint8_t result;
    int16_t score;
    uint16_t ply_count;
    uint8_t reserved;
} training_entry;

typedef struct {
    long games_completed;
    long positions_written;
    long bytes_written;
} datagen_progress;

static void init_engine_state(void) {
    static int initialized = 0;

    if (initialized)
        return;

    zobrist_fill();
    board_create_table();
    generate_PST();
    init_eval_tables(50);
    clear_heuristics();
    initialized = 1;
}

static int search_position_score(int depth) {
    search_start_ms = get_time_ms();
    time_per_move_ms = 60000;
    nodes_searched = 0;
    ended_early = 0;
    clear_heuristics();
    memset(table->table, 0, sizeof(hash_node) * table->size);
    return search(depth, -INFINITE, INFINITE, 0, 0);
}

static uint8_t encode_piece(unsigned piece, int color) {
    if (piece == EMPTY)
        return 0;
    return (uint8_t)(piece + (color == BLACK ? 6 : 0));
}

static void decode_piece(uint8_t code, unsigned *piece, int *color) {
    if (code == 0) {
        *piece = EMPTY;
        *color = EMPTY;
        return;
    }

    if (code > 6) {
        *piece = code - 6;
        *color = BLACK;
    } else {
        *piece = code;
        *color = WHITE;
    }
}

static uint8_t sq0x88_to_sq64(unsigned sq) {
    return (uint8_t)(INDEX2ROW(sq) * 8 + INDEX2COLUMN(sq));
}

static void pack_training_entry(training_entry *entry, int result, int score) {
    int sq64;

    memset(entry, 0, sizeof(*entry));
    for (sq64 = 0; sq64 < 64; sq64++) {
        unsigned sq = ROWCOLUMN2INDEX(sq64 / 8, sq64 % 8);
        uint8_t code = encode_piece(pieces[sq], colours[sq]);

        if ((sq64 & 1) == 0)
            entry->pieces[sq64 / 2] = code;
        else
            entry->pieces[sq64 / 2] |= (uint8_t)(code << 4);
    }

    entry->stm = (turn == WHITE) ? 0 : 1;
    entry->castling = (uint8_t)castling;
    entry->ep_square = (enpassant_target == NO_ENPASSANT) ? 64 : sq0x88_to_sq64(enpassant_target);
    entry->result = (uint8_t)result;
    if (score > 32767) score = 32767;
    if (score < -32768) score = -32768;
    entry->score = (int16_t)score;
    entry->ply_count = (uint16_t)((total_history > 65535) ? 65535 : total_history);
}

static int parse_fen_prefix(const char *line, char *fen, size_t fen_size) {
    char a[128], b[128], c[128], d[128];

    if (sscanf(line, "%127s %127s %127s %127s", a, b, c, d) != 4)
        return 0;

    snprintf(fen, fen_size, "%s %s %s %s 0 1", a, b, c, d);
    return 1;
}

static long append_seed_file(FILE *out, const char *path, int depth, long *positions_written) {
    FILE *fp;
    char line[2048];
    long count = 0;

    fp = fopen(path, "r");
    if (!fp)
        return 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char fen[512];
        training_entry entry;
        int score;

        if (!parse_fen_prefix(line, fen, sizeof(fen)))
            continue;

        board_set_fen(fen);
        score = search_position_score(depth);
        pack_training_entry(&entry, TRAINING_RESULT_UNKNOWN, score);
        fwrite(&entry, sizeof(entry), 1, out);
        count++;
        (*positions_written)++;
    }

    fclose(fp);
    return count;
}

static int legal_move_count(void) {
    unsigned moves[256];
    unsigned count, i;
    int legal = 0;

    count = gen_moves(moves);
    for (i = 0; i < count; i++) {
        if (board_add(moves[i])) {
            legal++;
            board_subtract();
        }
    }
    return legal;
}

static int enough_material(void) {
    return (int)(w_pieces.count + b_pieces.count) >= 4;
}

static int random_legal_move(unsigned *move_out) {
    unsigned moves[256];
    unsigned legal_moves[256];
    unsigned count, i, legal_count = 0;

    count = gen_moves(moves);
    for (i = 0; i < count; i++) {
        if (board_add(moves[i])) {
            board_subtract();
            legal_moves[legal_count++] = moves[i];
        }
    }

    if (!legal_count)
        return 0;

    *move_out = legal_moves[rand() % legal_count];
    return 1;
}

static int current_result_from_turn(void) {
    if (legal_move_count() > 0)
        return TRAINING_RESULT_DRAW;

    if (is_in_check(turn))
        return (turn == WHITE) ? TRAINING_RESULT_BLACK : TRAINING_RESULT_WHITE;

    return TRAINING_RESULT_DRAW;
}

static void save_progress(const char *path, const datagen_progress *progress) {
    FILE *fp = fopen(path, "w");

    if (!fp)
        return;

    fprintf(fp, "%ld %ld %ld\n", progress->games_completed, progress->positions_written,
            progress->bytes_written);
    fclose(fp);
}

static void load_progress(const char *path, datagen_progress *progress) {
    FILE *fp = fopen(path, "r");

    memset(progress, 0, sizeof(*progress));
    if (!fp)
        return;

    if (fscanf(fp, "%ld %ld %ld", &progress->games_completed, &progress->positions_written,
               &progress->bytes_written) != 3)
        memset(progress, 0, sizeof(*progress));
    fclose(fp);
}

static int run_datagen(int argc, char **argv) {
    const char *output = "data/gen.bin";
    char progress_path[1024];
    const char *seed_files[] = {
        "suites/perftsuite.epd",
        "suites/arasan12.epd",
        "suites/bt2630.epd",
        "suites/ecmgcp.epd",
        "suites/eet.epd",
        "suites/lapuce2.epd",
        "suites/pet.epd",
        "suites/sbd.epd",
        "suites/wac.epd",
        "suites/epd/STS1.epd",
        "suites/epd/STS2.epd",
        "suites/epd/STS3.epd",
        "suites/epd/STS4.epd",
        "suites/epd/STS5.epd",
        "suites/epd/STS6.epd",
        "suites/epd/STS7.epd",
        "suites/epd/STS8.epd",
        "suites/epd/STS9.epd",
        "suites/epd/STS10.epd",
        "suites/epd/STS11.epd",
        "suites/epd/STS12.epd",
        "suites/epd/STS13.epd"
    };
    FILE *out;
    int games = 1000;
    int i;
    datagen_progress progress;
    long seed_positions = 0;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--games") == 0 && i + 1 < argc)
            games = atoi(argv[++i]);
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output = argv[++i];
    }

    init_engine_state();
    srand((unsigned)time(NULL));

    snprintf(progress_path, sizeof(progress_path), "%s.progress", output);
    load_progress(progress_path, &progress);

    out = fopen(output, progress.bytes_written > 0 ? "ab" : "wb");
    if (!out) {
        fprintf(stderr, "failed to open %s: %s\n", output, strerror(errno));
        return 1;
    }

    if (progress.bytes_written == 0) {
        printf("[datagen] Labeling seed positions at depth 7...\n");
        for (i = 0; i < (int)(sizeof(seed_files) / sizeof(seed_files[0])); i++)
            seed_positions += append_seed_file(out, seed_files[i], 7, &progress.positions_written);
        progress.bytes_written = progress.positions_written * (long)sizeof(training_entry);
        save_progress(progress_path, &progress);
        printf("[datagen] Seed labeling complete: %ld positions\n", seed_positions);
    } else {
        printf("[datagen] Resuming from %ld completed games\n", progress.games_completed);
    }

    printf("[datagen] Starting self-play generation: %d games at depth 6\n", games);
    for (i = (int)progress.games_completed; i < games; i++) {
        training_entry game_entries[256];
        int game_entry_count = 0;
        int random_plies = 0;
        int decisive_streak = 0;
        int result = TRAINING_RESULT_DRAW;
        int ply = 0;

        board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        while (random_plies < 8) {
            unsigned move;
            if (!random_legal_move(&move) || !board_add(move))
                break;
            random_plies++;
            ply++;
        }

        while (ply < 100) {
            unsigned move;
            int score;
            int legal;

            legal = legal_move_count();
            if (legal == 0) {
                result = current_result_from_turn();
                break;
            }

            score = search_position_score(6);
            if (!is_in_check(turn) && enough_material() && ABS(score) <= 3000 &&
                game_entry_count < (int)(sizeof(game_entries) / sizeof(game_entries[0]))) {
                pack_training_entry(&game_entries[game_entry_count++], TRAINING_RESULT_UNKNOWN, score);
            }

            if (ABS(score) > 1000)
                decisive_streak++;
            else
                decisive_streak = 0;

            if (decisive_streak >= 5) {
                result = (score > 0)
                    ? (turn == WHITE ? TRAINING_RESULT_WHITE : TRAINING_RESULT_BLACK)
                    : (turn == WHITE ? TRAINING_RESULT_BLACK : TRAINING_RESULT_WHITE);
                break;
            }

            move = iterative_deepening(30, 6, 1);
            if (!move || !board_add(move)) {
                result = current_result_from_turn();
                break;
            }

            ply++;
        }

        if (ply >= 100)
            result = TRAINING_RESULT_DRAW;

        while (game_entry_count > 0) {
            game_entries[game_entry_count - 1].result = (uint8_t)result;
            fwrite(&game_entries[game_entry_count - 1], sizeof(training_entry), 1, out);
            progress.positions_written++;
            game_entry_count--;
        }

        progress.games_completed = i + 1;
        progress.bytes_written = progress.positions_written * (long)sizeof(training_entry);
        if ((i + 1) % 25 == 0 || i + 1 == games) {
            save_progress(progress_path, &progress);
            printf("[datagen]   %d/%d games (%.1f%%) - %ld positions written\n",
                   i + 1, games, 100.0 * (double)(i + 1) / (double)games, progress.positions_written);
        }
    }

    fclose(out);
    printf("[datagen] Complete: %ld games, %ld positions written to %s\n",
           progress.games_completed, progress.positions_written, output);
    return 0;
}

static int solve_linear_system(double a[6][7], double x[6]) {
    int i, j, k;

    for (i = 0; i < 6; i++) {
        int pivot = i;
        for (j = i + 1; j < 6; j++) {
            if (ABS(a[j][i]) > ABS(a[pivot][i]))
                pivot = j;
        }

        if (a[pivot][i] == 0.0)
            return 0;

        if (pivot != i) {
            for (k = i; k < 7; k++) {
                double tmp = a[i][k];
                a[i][k] = a[pivot][k];
                a[pivot][k] = tmp;
            }
        }

        for (j = i + 1; j < 6; j++) {
            double factor = a[j][i] / a[i][i];
            for (k = i; k < 7; k++)
                a[j][k] -= factor * a[i][k];
        }
    }

    for (i = 5; i >= 0; i--) {
        double sum = a[i][6];
        for (j = i + 1; j < 6; j++)
            sum -= a[i][j] * x[j];
        x[i] = sum / a[i][i];
    }

    return 1;
}

static void accumulate_material_features(const training_entry *entry, double feats[6]) {
    int sq64;
    int stm_color = (entry->stm == 0) ? WHITE : BLACK;
    int own_counts[7] = {0};
    int opp_counts[7] = {0};

    feats[0] = 1.0;
    for (sq64 = 0; sq64 < 64; sq64++) {
        uint8_t code = ((sq64 & 1) == 0)
            ? (entry->pieces[sq64 / 2] & 0x0F)
            : ((entry->pieces[sq64 / 2] >> 4) & 0x0F);
        unsigned piece;
        int color;

        decode_piece(code, &piece, &color);
        if (piece == EMPTY || piece == KING)
            continue;

        if (color == stm_color)
            own_counts[piece]++;
        else
            opp_counts[piece]++;
    }

    feats[1] = (double)(own_counts[PAWN] - opp_counts[PAWN]);
    feats[2] = (double)(own_counts[KNIGHT] - opp_counts[KNIGHT]);
    feats[3] = (double)(own_counts[BISHOP] - opp_counts[BISHOP]);
    feats[4] = (double)(own_counts[ROOK] - opp_counts[ROOK]);
    feats[5] = (double)(own_counts[QUEEN] - opp_counts[QUEEN]);
}

static int write_material_bootstrap_net(const char *path, const double weights[6]) {
    FILE *fp;
    nnue_header header;
    int16_t *ft_weights;
    int16_t ft_bias[NNUE_ACCUMULATOR_SIZE];
    int32_t hidden_bias[NNUE_HIDDEN_SIZE];
    int8_t hidden_weights[NNUE_ACCUMULATOR_SIZE * 2 * NNUE_HIDDEN_SIZE];
    int8_t output_weights[NNUE_HIDDEN_SIZE];
    int32_t output_bias;
    int king64, sq64;

    ft_weights = (int16_t *)calloc((size_t)NNUE_FT_INPUTS * NNUE_ACCUMULATOR_SIZE, sizeof(int16_t));
    if (!ft_weights)
        return 0;

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "CERUNNUE", 8);
    header.version = 1;
    header.ft_inputs = NNUE_FT_INPUTS;
    header.accumulator_size = NNUE_ACCUMULATOR_SIZE;
    header.hidden_size = NNUE_HIDDEN_SIZE;
    header.output_scale = 1;

    memset(ft_bias, 0, sizeof(ft_bias));
    memset(hidden_bias, 0, sizeof(hidden_bias));
    memset(hidden_weights, 0, sizeof(hidden_weights));
    memset(output_weights, 0, sizeof(output_weights));
    output_bias = (int32_t)weights[0];

    for (king64 = 0; king64 < 64; king64++) {
        for (sq64 = 0; sq64 < 64; sq64++) {
            int base = (king64 * 10) * 64 + sq64;

            ft_weights[(base + 0 * 64) * NNUE_ACCUMULATOR_SIZE + 0] = 1;
            ft_weights[(base + 1 * 64) * NNUE_ACCUMULATOR_SIZE + 1] = 1;
            ft_weights[(base + 2 * 64) * NNUE_ACCUMULATOR_SIZE + 2] = 1;
            ft_weights[(base + 3 * 64) * NNUE_ACCUMULATOR_SIZE + 3] = 1;
            ft_weights[(base + 4 * 64) * NNUE_ACCUMULATOR_SIZE + 4] = 1;
        }
    }

    for (sq64 = 0; sq64 < 5; sq64++) {
        hidden_weights[sq64 * NNUE_HIDDEN_SIZE + sq64] = 1;
        hidden_weights[(NNUE_ACCUMULATOR_SIZE + sq64) * NNUE_HIDDEN_SIZE + (5 + sq64)] = 1;
        int w = (int)weights[sq64 + 1];
        if (w > 127) w = 127;
        if (w < -127) w = -127;
        output_weights[sq64] = (int8_t)w;
        output_weights[5 + sq64] = (int8_t)(-w);
    }

    fp = fopen(path, "wb");
    if (!fp) {
        free(ft_weights);
        return 0;
    }

    fwrite(&header, sizeof(header), 1, fp);
    fwrite(ft_bias, sizeof(int16_t), NNUE_ACCUMULATOR_SIZE, fp);
    fwrite(ft_weights, sizeof(int16_t), NNUE_FT_INPUTS * NNUE_ACCUMULATOR_SIZE, fp);
    fwrite(hidden_bias, sizeof(int32_t), NNUE_HIDDEN_SIZE, fp);
    fwrite(hidden_weights, sizeof(int8_t), NNUE_ACCUMULATOR_SIZE * 2 * NNUE_HIDDEN_SIZE, fp);
    fwrite(&output_bias, sizeof(int32_t), 1, fp);
    fwrite(output_weights, sizeof(int8_t), NNUE_HIDDEN_SIZE, fp);
    fclose(fp);
    free(ft_weights);
    return 1;
}

static int run_train(int argc, char **argv) {
    const char *data = "data/gen.bin";
    const char *net = "nets/default.nnue";
    FILE *fp;
    training_entry entry;
    double xtx[6][7];
    double solution[6] = {0};
    long count = 0;
    int i, j;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc)
            data = argv[++i];
        else if (strcmp(argv[i], "--net") == 0 && i + 1 < argc)
            net = argv[++i];
    }

    memset(xtx, 0, sizeof(xtx));
    fp = fopen(data, "rb");
    if (!fp) {
        fprintf(stderr, "failed to open %s: %s\n", data, strerror(errno));
        return 1;
    }

    while (fread(&entry, sizeof(entry), 1, fp) == 1) {
        double feats[6];
        accumulate_material_features(&entry, feats);
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 6; j++)
                xtx[i][j] += feats[i] * feats[j];
            xtx[i][6] += feats[i] * entry.score;
        }
        count++;
    }
    fclose(fp);

    if (!count) {
        fprintf(stderr, "no training rows found in %s\n", data);
        return 1;
    }

    for (i = 0; i < 6; i++)
        xtx[i][i] += 1e-3;

    if (!solve_linear_system(xtx, solution)) {
        fprintf(stderr, "failed to solve bootstrap regression\n");
        return 1;
    }

    if (!write_material_bootstrap_net(net, solution)) {
        fprintf(stderr, "failed to write %s\n", net);
        return 1;
    }

    printf("[train] Loaded %ld positions from %s\n", count, data);
    printf("[train] Bootstrap material weights: bias=%.1f pawn=%.1f knight=%.1f bishop=%.1f rook=%.1f queen=%.1f\n",
           solution[0], solution[1], solution[2], solution[3], solution[4], solution[5]);
    printf("[train] Wrote %s\n", net);
    return 0;
}

static int run_sts_eval(int argc, char **argv) {
    const char *net = "nets/default.nnue";
    int i;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--net") == 0 && i + 1 < argc)
            net = argv[++i];
    }

    init_engine_state();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!nnue_load(net)) {
        fprintf(stderr, "failed to load %s\n", net);
        return 1;
    }

    max_depth = 64;
    printf("[sts-eval] Loading net: %s\n", net);
    printf("[sts-eval] Running STS at 100ms/move\n");
    printf("[sts-eval] Total: %d\n", sts_run(100));
    return 0;
}

int cli_run(int argc, char **argv) {
    if (argc < 2)
        return -1;

    if (strcmp(argv[1], "--datagen") == 0)
        return run_datagen(argc, argv);
    if (strcmp(argv[1], "--train") == 0)
        return run_train(argc, argv);
    if (strcmp(argv[1], "--sts-eval") == 0)
        return run_sts_eval(argc, argv);

    return -1;
}
