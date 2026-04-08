#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
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

typedef struct {
    int worker_id;
    int assigned_games;
    char shard_path[1024];
    char progress_path[1060];
#ifndef _WIN32
    pid_t pid;
#endif
    int completed;
} datagen_worker;

static void format_duration_ms(long ms, char *buf, size_t buf_size) {
    long total_seconds;
    long hours, minutes, seconds;

    if (ms < 0)
        ms = 0;

    total_seconds = ms / 1000;
    hours = total_seconds / 3600;
    minutes = (total_seconds % 3600) / 60;
    seconds = total_seconds % 60;

    if (hours > 0)
        snprintf(buf, buf_size, "%ldh%02ldm%02lds", hours, minutes, seconds);
    else if (minutes > 0)
        snprintf(buf, buf_size, "%ldm%02lds", minutes, seconds);
    else
        snprintf(buf, buf_size, "%lds", seconds);
}

static long estimate_remaining_ms(long elapsed_ms, long done, long total) {
    if (elapsed_ms <= 0 || done <= 0 || total <= done)
        return 0;

    return (long)((double)elapsed_ms * (double)(total - done) / (double)done);
}

static void print_seed_progress(long done, long total, long elapsed_ms) {
    double pct = (total > 0) ? (100.0 * (double)done / (double)total) : 0.0;
    double per_second = (elapsed_ms > 0) ? ((double)done * 1000.0 / (double)elapsed_ms) : 0.0;
    long remaining_ms = estimate_remaining_ms(elapsed_ms, done, total);
    char elapsed_buf[32];
    char eta_buf[32];

    format_duration_ms(elapsed_ms, elapsed_buf, sizeof(elapsed_buf));
    format_duration_ms(remaining_ms, eta_buf, sizeof(eta_buf));
    printf("[datagen] Seed labeling %ld/%ld (%.1f%%) - %.1f pos/s - elapsed %s - ETA %s\n",
           done, total, pct, per_second, elapsed_buf, eta_buf);
    fflush(stdout);
}

static void print_parallel_selfplay_progress(long done_games, long total_games, long total_positions,
                                             long elapsed_ms, int active_workers) {
    double pct = (total_games > 0) ? (100.0 * (double)done_games / (double)total_games) : 0.0;
    double games_per_min = (elapsed_ms > 0) ? ((double)done_games * 60000.0 / (double)elapsed_ms) : 0.0;
    double positions_per_second = (elapsed_ms > 0)
        ? ((double)total_positions * 1000.0 / (double)elapsed_ms) : 0.0;
    long remaining_ms = estimate_remaining_ms(elapsed_ms, done_games, total_games);
    char elapsed_buf[32];
    char eta_buf[32];

    format_duration_ms(elapsed_ms, elapsed_buf, sizeof(elapsed_buf));
    format_duration_ms(remaining_ms, eta_buf, sizeof(eta_buf));
    printf("[datagen] Self-play %ld/%ld games (%.1f%%) - %ld positions - %.2f games/min - %.1f pos/s - active workers %d - elapsed %s - ETA %s\n",
           done_games, total_games, pct, total_positions, games_per_min, positions_per_second,
           active_workers, elapsed_buf, eta_buf);
    fflush(stdout);
}

static void print_game_heartbeat(int game_number, int total_games, int ply, int sampled_positions,
                                 long elapsed_ms, long total_positions, long selfplay_elapsed_ms) {
    double positions_per_second = (selfplay_elapsed_ms > 0)
        ? ((double)total_positions * 1000.0 / (double)selfplay_elapsed_ms) : 0.0;
    char elapsed_buf[32];

    format_duration_ms(elapsed_ms, elapsed_buf, sizeof(elapsed_buf));
    printf("[datagen]   game %d/%d still running - ply %d - sampled %d positions - elapsed %s - self-play %.1f pos/s\n",
           game_number, total_games, ply, sampled_positions, elapsed_buf, positions_per_second);
    fflush(stdout);
}

static int datagen_default_workers(void) {
#ifdef _WIN32
    return 1;
#else
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1)
        return 1;
    if (cpus > 8)
        cpus = 8;
    return (int)cpus;
#endif
}

static void datagen_seed_path(char *buf, size_t buf_size, const char *output) {
    snprintf(buf, buf_size, "%s.seed.bin", output);
}

static void datagen_shard_path(char *buf, size_t buf_size, const char *output, int worker_id) {
    snprintf(buf, buf_size, "%s.shard%02d.bin", output, worker_id);
}

static void datagen_progress_path(char *buf, size_t buf_size, const char *data_path) {
    snprintf(buf, buf_size, "%s.progress", data_path);
}

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

static long append_seed_file(FILE *out, const char *path, int depth, long *positions_written,
                             long *seed_done, long seed_total, long seed_start_ms) {
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
        (*seed_done)++;

        if ((*seed_done % 100) == 0 || *seed_done == seed_total)
            print_seed_progress(*seed_done, seed_total, get_time_ms() - seed_start_ms);
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

static long file_size_or_zero(const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;

    if (!fp)
        return 0;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }

    size = ftell(fp);
    fclose(fp);
    return (size < 0) ? 0 : size;
}

static int copy_file_into(FILE *out, const char *path) {
    FILE *in = fopen(path, "rb");
    char buffer[1 << 15];
    size_t nread;

    if (!in)
        return 0;

    while ((nread = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, nread, out) != nread) {
            fclose(in);
            return 0;
        }
    }

    fclose(in);
    return 1;
}

static int merge_datagen_outputs(const char *output, const char *seed_path,
                                 datagen_worker *workers, int worker_count) {
    FILE *out = fopen(output, "wb");
    int i;

    if (!out)
        return 0;

    if (!copy_file_into(out, seed_path)) {
        fclose(out);
        return 0;
    }

    for (i = 0; i < worker_count; i++) {
        if (!copy_file_into(out, workers[i].shard_path)) {
            fclose(out);
            return 0;
        }
    }

    fclose(out);
    return 1;
}

static int ensure_seed_file(const char *seed_path, const char **seed_files, int seed_file_count,
                            long seed_total) {
    FILE *out;
    datagen_progress progress;
    long seed_positions = 0;
    long seed_done = 0;
    long seed_start_ms;
    int i;

    if (file_size_or_zero(seed_path) == (long)(seed_total * (long)sizeof(training_entry))) {
        printf("[datagen] Seed file ready: %s (%ld positions)\n", seed_path, seed_total);
        fflush(stdout);
        return 1;
    }

    memset(&progress, 0, sizeof(progress));
    out = fopen(seed_path, "wb");
    if (!out) {
        fprintf(stderr, "failed to open %s: %s\n", seed_path, strerror(errno));
        return 0;
    }

    printf("[datagen] Labeling seed positions at depth 7...\n");
    fflush(stdout);
    seed_start_ms = get_time_ms();
    for (i = 0; i < seed_file_count; i++) {
        seed_positions += append_seed_file(out, seed_files[i], 7, &progress.positions_written,
                                           &seed_done, seed_total, seed_start_ms);
    }
    fclose(out);

    printf("[datagen] Seed labeling complete: %ld positions in ", seed_positions);
    {
        char elapsed_buf[32];
        format_duration_ms(get_time_ms() - seed_start_ms, elapsed_buf, sizeof(elapsed_buf));
        printf("%s\n", elapsed_buf);
    }
    fflush(stdout);
    return 1;
}

static int run_datagen_worker(const char *output, int games, unsigned seed,
                              int worker_id, int verbose_heartbeat,
                              const char *selfplay_net) {
    char progress_path[1060];
    FILE *out;
    datagen_progress progress;
    int i;
    long selfplay_start_ms;
    long selfplay_start_positions;

    datagen_progress_path(progress_path, sizeof(progress_path), output);
    (void)worker_id;
    load_progress(progress_path, &progress);

    out = fopen(output, progress.bytes_written > 0 ? "ab" : "wb");
    if (!out) {
        fprintf(stderr, "failed to open %s: %s\n", output, strerror(errno));
        return 1;
    }

    init_engine_state();
    if (selfplay_net && selfplay_net[0]) {
        board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        if (!nnue_load(selfplay_net)) {
            fprintf(stderr, "failed to load self-play net %s\n", selfplay_net);
            fclose(out);
            return 1;
        }
    }
    srand(seed);
    selfplay_start_ms = get_time_ms();
    selfplay_start_positions = progress.positions_written;

    for (i = (int)progress.games_completed; i < games; i++) {
        training_entry game_entries[256];
        int game_entry_count = 0;
        int random_plies = 0;
        int decisive_streak = 0;
        int result = TRAINING_RESULT_DRAW;
        int ply = 0;
        long game_start_ms = get_time_ms();
        long last_heartbeat_ms = game_start_ms;

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
            long now_ms = get_time_ms();

            if (verbose_heartbeat && now_ms - last_heartbeat_ms >= 5000) {
                print_game_heartbeat(i + 1, games, ply, game_entry_count, now_ms - game_start_ms,
                                     progress.positions_written - selfplay_start_positions,
                                     now_ms - selfplay_start_ms);
                last_heartbeat_ms = now_ms;
            }

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
        if ((i + 1) % 10 == 0 || i + 1 == games)
            save_progress(progress_path, &progress);
    }

    fclose(out);
    save_progress(progress_path, &progress);
    return 0;
}

#ifndef _WIN32
static int spawn_datagen_worker(const char *self_path, datagen_worker *worker, unsigned seed,
                                const char *selfplay_net) {
    pid_t pid = fork();
    char games_arg[32];
    char seed_arg[32];
    char worker_arg[32];

    if (pid < 0)
        return 0;

    if (pid == 0) {
        snprintf(games_arg, sizeof(games_arg), "%d", worker->assigned_games);
        snprintf(seed_arg, sizeof(seed_arg), "%u", seed);
        snprintf(worker_arg, sizeof(worker_arg), "%d", worker->worker_id);
        if (selfplay_net && selfplay_net[0]) {
            execl(self_path, self_path,
                  "--datagen-worker",
                  "--games", games_arg,
                  "--output", worker->shard_path,
                  "--seed", seed_arg,
                  "--worker-id", worker_arg,
                  "--selfplay-net", selfplay_net,
                  (char *)NULL);
        } else {
            execl(self_path, self_path,
                  "--datagen-worker",
                  "--games", games_arg,
                  "--output", worker->shard_path,
                  "--seed", seed_arg,
                  "--worker-id", worker_arg,
                  (char *)NULL);
        }
        _exit(127);
    }

    worker->pid = pid;
    return 1;
}
#endif

static int run_datagen_parent(const char *self_path, const char *output, int games,
                              int workers_requested, const char *selfplay_net) {
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
    char seed_path[1024];
    int worker_count;
    datagen_worker *workers;
    int i;
    int base_games;
    int extra_games;
    int active_workers;
    long selfplay_start_ms;
    long last_report_ms;
    long previous_done = -1;
    long previous_positions = -1;
    long seed_total = 2463;

    worker_count = (workers_requested > 0) ? workers_requested : datagen_default_workers();
    if (worker_count < 1)
        worker_count = 1;
    if (worker_count > games && games > 0)
        worker_count = games;
    if (worker_count < 1)
        worker_count = 1;

    datagen_seed_path(seed_path, sizeof(seed_path), output);
    init_engine_state();
    if (!ensure_seed_file(seed_path, seed_files, (int)(sizeof(seed_files) / sizeof(seed_files[0])), seed_total))
        return 1;

    workers = (datagen_worker *)calloc((size_t)worker_count, sizeof(datagen_worker));
    if (!workers)
        return 1;

    base_games = (worker_count > 0) ? (games / worker_count) : games;
    extra_games = (worker_count > 0) ? (games % worker_count) : 0;
    active_workers = 0;

    for (i = 0; i < worker_count; i++) {
        datagen_progress shard_progress;
        workers[i].worker_id = i + 1;
        workers[i].assigned_games = base_games + (i < extra_games ? 1 : 0);
        datagen_shard_path(workers[i].shard_path, sizeof(workers[i].shard_path), output, workers[i].worker_id);
        datagen_progress_path(workers[i].progress_path, sizeof(workers[i].progress_path), workers[i].shard_path);
        load_progress(workers[i].progress_path, &shard_progress);
        if (shard_progress.games_completed >= workers[i].assigned_games)
            workers[i].completed = 1;
    }

    if (selfplay_net && selfplay_net[0])
        printf("[datagen] Starting self-play generation: %d games at depth 6 with %d worker(s) using %s\n",
               games, worker_count, selfplay_net);
    else
        printf("[datagen] Starting self-play generation: %d games at depth 6 with %d worker(s)\n",
               games, worker_count);
    fflush(stdout);

#ifdef _WIN32
    if (worker_count > 1)
        printf("[datagen] Parallel workers are not enabled on this build; falling back to 1 worker\n");
    free(workers);
        return run_datagen_worker(output, games, (unsigned)time(NULL), 1, 1, selfplay_net);
#else
    for (i = 0; i < worker_count; i++) {
        if (workers[i].completed || workers[i].assigned_games <= 0)
            continue;
        if (!spawn_datagen_worker(self_path, &workers[i],
                                  (unsigned)time(NULL) ^ (unsigned)(i * 2654435761u),
                                  selfplay_net)) {
            fprintf(stderr, "failed to spawn datagen worker %d\n", i + 1);
            free(workers);
            return 1;
        }
        active_workers++;
    }

    selfplay_start_ms = get_time_ms();
    last_report_ms = 0;
    while (active_workers > 0) {
        long done_games = 0;
        long done_positions = 0;
        long now_ms = get_time_ms();

        for (i = 0; i < worker_count; i++) {
            datagen_progress shard_progress;
            load_progress(workers[i].progress_path, &shard_progress);
            if (shard_progress.games_completed > workers[i].assigned_games)
                shard_progress.games_completed = workers[i].assigned_games;
            done_games += shard_progress.games_completed;
            done_positions += shard_progress.positions_written;
        }

        if (now_ms - last_report_ms >= 2000 ||
            done_games != previous_done || done_positions != previous_positions) {
            int live_workers = 0;
            for (i = 0; i < worker_count; i++) {
                if (!workers[i].completed && workers[i].assigned_games > 0)
                    live_workers++;
            }
            print_parallel_selfplay_progress(done_games, games, done_positions,
                                             now_ms - selfplay_start_ms, live_workers);
            previous_done = done_games;
            previous_positions = done_positions;
            last_report_ms = now_ms;
        }

        for (i = 0; i < worker_count; i++) {
            int status;
            pid_t result;

            if (workers[i].completed || workers[i].assigned_games <= 0)
                continue;

            result = waitpid(workers[i].pid, &status, WNOHANG);
            if (result == workers[i].pid) {
                workers[i].completed = 1;
                active_workers--;
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    fprintf(stderr, "datagen worker %d failed\n", workers[i].worker_id);
                    free(workers);
                    return 1;
                }
            }
        }

        usleep(250000);
    }

    if (!merge_datagen_outputs(output, seed_path, workers, worker_count)) {
        fprintf(stderr, "failed to merge datagen outputs into %s\n", output);
        free(workers);
        return 1;
    }

    {
        long total_positions = seed_total;
        for (i = 0; i < worker_count; i++) {
            datagen_progress shard_progress;
            load_progress(workers[i].progress_path, &shard_progress);
            total_positions += shard_progress.positions_written;
        }
        printf("[datagen] Complete: %d games, %ld positions written to %s\n",
               games, total_positions, output);
        fflush(stdout);
    }

    free(workers);
    return 0;
#endif
}

static int run_datagen(int argc, char **argv) {
    const char *output = "data/gen.bin";
    const char *self_path = argv[0];
    const char *selfplay_net = NULL;
    int games = 1000;
    int workers = 0;
    int i;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--games") == 0 && i + 1 < argc)
            games = atoi(argv[++i]);
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc)
            workers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--selfplay-net") == 0 && i + 1 < argc)
            selfplay_net = argv[++i];
    }

    return run_datagen_parent(self_path, output, games, workers, selfplay_net);
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

static int train_net_from_file(const char *data, const char *net) {
    FILE *fp;
    training_entry entry;
    double xtx[6][7];
    double solution[6] = {0};
    long count = 0;
    long data_size = 0;
    long read_start_ms = 0;
    long write_start_ms = 0;
    int i, j;

    memset(xtx, 0, sizeof(xtx));
    fp = fopen(data, "rb");
    if (!fp) {
        fprintf(stderr, "failed to open %s: %s\n", data, strerror(errno));
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) == 0) {
        data_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    printf("[train] Loading training data: %s", data);
    if (data_size > 0)
        printf(" (%ld positions, %.1f MB)\n", data_size / (long)sizeof(training_entry),
               (double)data_size / (1024.0 * 1024.0));
    else
        printf("\n");
    printf("[train] Network: 40960 -> 128 -> 32 -> 1 (bootstrap material fit)\n");
    fflush(stdout);
    read_start_ms = get_time_ms();

    while (fread(&entry, sizeof(entry), 1, fp) == 1) {
        double feats[6];
        accumulate_material_features(&entry, feats);
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 6; j++)
                xtx[i][j] += feats[i] * feats[j];
            xtx[i][6] += feats[i] * entry.score;
        }
        count++;

        if (count % 100000 == 0) {
            long elapsed_ms = get_time_ms() - read_start_ms;
            long total_positions = (data_size > 0) ? (data_size / (long)sizeof(training_entry)) : 0;
            double pos_per_second = (elapsed_ms > 0)
                ? ((double)count * 1000.0 / (double)elapsed_ms) : 0.0;
            long remaining_ms = estimate_remaining_ms(elapsed_ms, count, total_positions);
            char elapsed_buf[32];
            char eta_buf[32];

            format_duration_ms(elapsed_ms, elapsed_buf, sizeof(elapsed_buf));
            format_duration_ms(remaining_ms, eta_buf, sizeof(eta_buf));
            if (total_positions > 0) {
                printf("[train] Read %ld/%ld positions (%.1f%%) - %.1f pos/s - elapsed %s - ETA %s\n",
                       count, total_positions, 100.0 * (double)count / (double)total_positions,
                       pos_per_second, elapsed_buf, eta_buf);
            } else {
                printf("[train] Read %ld positions - %.1f pos/s - elapsed %s\n",
                       count, pos_per_second, elapsed_buf);
            }
            fflush(stdout);
        }
    }
    fclose(fp);

    {
        long elapsed_ms = get_time_ms() - read_start_ms;
        double pos_per_second = (elapsed_ms > 0)
            ? ((double)count * 1000.0 / (double)elapsed_ms) : 0.0;
        char elapsed_buf[32];

        format_duration_ms(elapsed_ms, elapsed_buf, sizeof(elapsed_buf));
        printf("[train] Data scan complete: %ld positions - %.1f pos/s - elapsed %s\n",
               count, pos_per_second, elapsed_buf);
        fflush(stdout);
    }

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

    printf("[train] Solved bootstrap regression, writing net...\n");
    fflush(stdout);
    write_start_ms = get_time_ms();
    if (!write_material_bootstrap_net(net, solution)) {
        fprintf(stderr, "failed to write %s\n", net);
        return 1;
    }

    printf("[train] Bootstrap material weights: bias=%.1f pawn=%.1f knight=%.1f bishop=%.1f rook=%.1f queen=%.1f\n",
           solution[0], solution[1], solution[2], solution[3], solution[4], solution[5]);
    {
        char write_elapsed_buf[32];
        char total_elapsed_buf[32];
        format_duration_ms(get_time_ms() - write_start_ms, write_elapsed_buf, sizeof(write_elapsed_buf));
        format_duration_ms(get_time_ms() - read_start_ms, total_elapsed_buf, sizeof(total_elapsed_buf));
        printf("[train] Wrote %s in %s\n", net, write_elapsed_buf);
        printf("[train] Training complete - elapsed %s\n", total_elapsed_buf);
    }
    fflush(stdout);
    return 0;
}

static int run_train(int argc, char **argv) {
    const char *data = "data/gen.bin";
    const char *net = "nets/default.nnue";
    int i;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc)
            data = argv[++i];
        else if (strcmp(argv[i], "--net") == 0 && i + 1 < argc)
            net = argv[++i];
    }

    return train_net_from_file(data, net);
}

static int sts_score_net_file(const char *net) {
    int score;

    init_engine_state();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!nnue_load(net)) {
        fprintf(stderr, "failed to load %s\n", net);
        return -1;
    }

    max_depth = 64;
    printf("[sts-eval] Loading net: %s\n", net);
    printf("[sts-eval] Running STS at 100ms/move\n");
    fflush(stdout);
    score = sts_run(100);
    printf("[sts-eval] Total: %d\n", score);
    fflush(stdout);
    return score;
}

static int run_sts_eval(int argc, char **argv) {
    const char *net = "nets/default.nnue";
    int i;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--net") == 0 && i + 1 < argc)
            net = argv[++i];
    }

    return (sts_score_net_file(net) < 0) ? 1 : 0;
}

static int run_datagen_worker_cli(int argc, char **argv) {
    const char *output = NULL;
    const char *selfplay_net = NULL;
    int games = 0;
    int worker_id = 0;
    unsigned seed = (unsigned)time(NULL);
    int i;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--games") == 0 && i + 1 < argc)
            games = atoi(argv[++i]);
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--worker-id") == 0 && i + 1 < argc)
            worker_id = atoi(argv[++i]);
        else if (strcmp(argv[i], "--selfplay-net") == 0 && i + 1 < argc)
            selfplay_net = argv[++i];
    }

    if (!output || games < 0) {
        fprintf(stderr, "invalid datagen worker arguments\n");
        return 1;
    }

    return run_datagen_worker(output, games, seed ^ (unsigned)(worker_id * 2246822519u),
                              worker_id, 0, selfplay_net);
}

static int run_nnue_loop(int argc, char **argv) {
    const char *self_path = argv[0];
    const char *start_net = "nets/gen1.nnue";
    const char *data_prefix = "data/gen";
    const char *net_prefix = "nets/gen";
    int start_round = 1;
    int rounds = 3;
    int games = 20000;
    int workers = 0;
    int i;
    char current_net[1024];

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--start-net") == 0 && i + 1 < argc)
            start_net = argv[++i];
        else if (strcmp(argv[i], "--start-round") == 0 && i + 1 < argc)
            start_round = atoi(argv[++i]);
        else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc)
            rounds = atoi(argv[++i]);
        else if (strcmp(argv[i], "--games") == 0 && i + 1 < argc)
            games = atoi(argv[++i]);
        else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc)
            workers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--data-prefix") == 0 && i + 1 < argc)
            data_prefix = argv[++i];
        else if (strcmp(argv[i], "--net-prefix") == 0 && i + 1 < argc)
            net_prefix = argv[++i];
    }

    if (rounds < 1)
        rounds = 1;
    if (start_round < 1)
        start_round = 1;

    strncpy(current_net, start_net, sizeof(current_net) - 1);
    current_net[sizeof(current_net) - 1] = '\0';

    printf("[nnue-loop] Starting from %s (round %d), running %d iteration(s)\n",
           current_net, start_round, rounds);
    fflush(stdout);

    for (i = 0; i < rounds; i++) {
        int next_round = start_round + i + 1;
        char data_path[1024];
        char net_path[1024];

        snprintf(data_path, sizeof(data_path), "%s%d.bin", data_prefix, next_round);
        snprintf(net_path, sizeof(net_path), "%s%d.nnue", net_prefix, next_round);

        printf("[nnue-loop] Round %d: self-play with %s -> %s -> %s\n",
               next_round, current_net, data_path, net_path);
        fflush(stdout);

        if (run_datagen_parent(self_path, data_path, games, workers, current_net) != 0)
            return 1;
        if (train_net_from_file(data_path, net_path) != 0)
            return 1;
        if (sts_score_net_file(net_path) < 0)
            return 1;

        strncpy(current_net, net_path, sizeof(current_net) - 1);
        current_net[sizeof(current_net) - 1] = '\0';
    }

    printf("[nnue-loop] Complete. Latest net: %s\n", current_net);
    fflush(stdout);
    return 0;
}

static int run_nnue_forever(int argc, char **argv) {
    const char *self_path = argv[0];
    const char *start_net = "nets/gen1.nnue";
    const char *data_prefix = "data/gen";
    const char *net_prefix = "nets/gen";
    int current_round = 1;
    int games = 20000;
    int workers = 0;
    int min_delta = 5;
    int patience = 2;
    int no_improve_rounds = 0;
    int best_score;
    int i;
    char current_net[1024];

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--start-net") == 0 && i + 1 < argc)
            start_net = argv[++i];
        else if (strcmp(argv[i], "--start-round") == 0 && i + 1 < argc)
            current_round = atoi(argv[++i]);
        else if (strcmp(argv[i], "--games") == 0 && i + 1 < argc)
            games = atoi(argv[++i]);
        else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc)
            workers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--min-delta") == 0 && i + 1 < argc)
            min_delta = atoi(argv[++i]);
        else if (strcmp(argv[i], "--patience") == 0 && i + 1 < argc)
            patience = atoi(argv[++i]);
        else if (strcmp(argv[i], "--data-prefix") == 0 && i + 1 < argc)
            data_prefix = argv[++i];
        else if (strcmp(argv[i], "--net-prefix") == 0 && i + 1 < argc)
            net_prefix = argv[++i];
    }

    if (current_round < 1)
        current_round = 1;

    strncpy(current_net, start_net, sizeof(current_net) - 1);
    current_net[sizeof(current_net) - 1] = '\0';

    printf("[nnue-forever] Starting from %s (round %d)\n", current_net, current_round);
    printf("[nnue-forever] Policy: min_delta=%d, patience=%d\n", min_delta, patience);
    fflush(stdout);

    best_score = sts_score_net_file(current_net);
    if (best_score < 0)
        return 1;
    printf("[nnue-forever] Baseline STS for %s: %d\n", current_net, best_score);
    fflush(stdout);

    while (1) {
        char data_path[1024];
        char net_path[1024];
        int next_round = current_round + 1;
        int score;

        snprintf(data_path, sizeof(data_path), "%s%d.bin", data_prefix, next_round);
        snprintf(net_path, sizeof(net_path), "%s%d.nnue", net_prefix, next_round);

        printf("[nnue-forever] Round %d: self-play with %s -> %s -> %s\n",
               next_round, current_net, data_path, net_path);
        fflush(stdout);

        if (run_datagen_parent(self_path, data_path, games, workers, current_net) != 0)
            return 1;
        if (train_net_from_file(data_path, net_path) != 0)
            return 1;
        score = sts_score_net_file(net_path);
        if (score < 0)
            return 1;

        if (score >= best_score + min_delta) {
            printf("[nnue-forever] Accepted round %d: STS %d -> %d\n",
                   next_round, best_score, score);
            best_score = score;
            no_improve_rounds = 0;
        } else {
            no_improve_rounds++;
            printf("[nnue-forever] No significant improvement at round %d: score=%d, best=%d, streak=%d/%d\n",
                   next_round, score, best_score, no_improve_rounds, patience);
            if (no_improve_rounds >= patience) {
                printf("[nnue-forever] Stopping: patience exhausted\n");
                fflush(stdout);
                return 0;
            }
        }
        fflush(stdout);

        strncpy(current_net, net_path, sizeof(current_net) - 1);
        current_net[sizeof(current_net) - 1] = '\0';
        current_round = next_round;
    }
}

int cli_run(int argc, char **argv) {
    if (argc < 2)
        return -1;

    if (strcmp(argv[1], "--datagen-worker") == 0)
        return run_datagen_worker_cli(argc, argv);
    if (strcmp(argv[1], "--datagen") == 0)
        return run_datagen(argc, argv);
    if (strcmp(argv[1], "--nnue-loop") == 0)
        return run_nnue_loop(argc, argv);
    if (strcmp(argv[1], "--nnue-forever") == 0)
        return run_nnue_forever(argc, argv);
    if (strcmp(argv[1], "--train") == 0)
        return run_train(argc, argv);
    if (strcmp(argv[1], "--sts-eval") == 0)
        return run_sts_eval(argc, argv);

    return -1;
}
