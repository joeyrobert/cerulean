/* sts.c - Strategic Test Suite runner
 *
 * Reads STS1.epd through STS10.epd from suites/epd/,
 * runs each position for time_per_move_ms, scores the result,
 * and reports total points.
 *
 * EPD format: <FEN> bm <move>; id "<id>"; c0 "<move>=<pts>, ...";
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sts.h"
#include "board.h"
#include "search.h"
#include "util.h"
#include "evaluate.h"

#define STS_SUITES 10
#define STS_MAX_LINE 512

/* Parse c0 field for points of a given move string.
 * c0 format: "Nf3=10, e4=5, ..."
 * Returns 0 if move not found.
 */
static int get_points(const char *c0_content, const char *move_str) {
    char buf[256];
    char *token;
    strncpy(buf, c0_content, sizeof(buf) - 1);
    buf[sizeof(buf)-1] = '\0';

    token = strtok(buf, ", ");
    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(token, move_str) == 0)
                return atoi(eq + 1);
        }
        token = strtok(NULL, ", ");
    }
    return 0;
}

int sts_run(int time_ms) {
    char filename[64];
    char line[STS_MAX_LINE];
    char fen[STS_MAX_LINE];
    char id_str[128];
    char c0_content[256];
    FILE *f;
    int suite, total_points = 0, total_positions = 0;
    unsigned best_move;
    char best_str[16];
    int pts;

    if (time_ms <= 0) time_ms = 1000;

    printf("Running Strategic Test Suite (time: %dms per position)\n\n", time_ms);

    long wall_start = get_time_ms();

    for (suite = 1; suite <= STS_SUITES; suite++) {
        snprintf(filename, sizeof(filename), "suites/epd/STS%d.epd", suite);
        f = fopen(filename, "r");
        if (!f) {
            printf("STS file not found at %s\n", filename);
            return total_points;
        }

        while (fgets(line, sizeof(line), f)) {
            /* Strip trailing newline */
            int len = (int)strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len == 0) continue;

            /* Extract FEN: everything before " bm " */
            char *bm_pos = strstr(line, " bm ");
            if (!bm_pos) continue;
            int fen_len = (int)(bm_pos - line);
            strncpy(fen, line, fen_len);
            fen[fen_len] = '\0';
            /* Append " 0 1" for FEN completeness */
            strncat(fen, " 0 1", sizeof(fen) - strlen(fen) - 1);

            /* Extract id */
            id_str[0] = '\0';
            const char *id_ptr = strstr(line, "id \"");
            if (id_ptr) {
                id_ptr += 4;
                const char *id_end = strchr(id_ptr, '"');
                if (id_end) {
                    int il = (int)(id_end - id_ptr);
                    if (il >= (int)sizeof(id_str)) il = (int)sizeof(id_str) - 1;
                    strncpy(id_str, id_ptr, il);
                    id_str[il] = '\0';
                }
            }

            /* Extract c0 content */
            c0_content[0] = '\0';
            const char *c0_ptr = strstr(line, "c0 \"");
            if (c0_ptr) {
                c0_ptr += 4;
                const char *c0_end = strchr(c0_ptr, '"');
                if (c0_end) {
                    int cl = (int)(c0_end - c0_ptr);
                    if (cl >= (int)sizeof(c0_content)) cl = (int)sizeof(c0_content) - 1;
                    strncpy(c0_content, c0_ptr, cl);
                    c0_content[cl] = '\0';
                }
            }

            if (c0_content[0] == '\0') continue;

            /* Set position and search */
            board_set_fen(fen);
            best_move = iterative_deepening(time_ms, max_depth, 1);

            /* Convert to short algebraic */
            if (best_move) {
                move_to_short_algebraic(best_move, best_str);
            } else {
                strcpy(best_str, "(none)");
            }

            pts = get_points(c0_content, best_str);
            total_points += pts;
            total_positions++;

            printf("%s\n", id_str);
            printf("%d/%d: %s\n", total_positions, STS_SUITES * 100, fen);
            printf("Move: %s  Points Added: %d  Total Points: %d\n\n",
                   best_str, pts, total_points);
        }

        fclose(f);
    }

    double duration = (get_time_ms() - wall_start) / 1000.0;
    printf("STS Total Points: %d / %d\n", total_points, STS_SUITES * 100 * 10);
    printf("Duration: %.3fs\n", duration);

    return total_points;
}
