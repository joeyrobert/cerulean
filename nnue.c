#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "nnue.h"
#include "board.h"
#include "move.h"

#define NNUE_MAGIC "CERUNNUE"
#define NNUE_VERSION 1u

typedef struct {
    int loaded;
    int enabled;
    int output_scale;
    char path[1024];
    int16_t *ft_weights;
    int16_t ft_bias[NNUE_ACCUMULATOR_SIZE];
    int8_t hidden_weights[NNUE_ACCUMULATOR_SIZE * 2 * NNUE_HIDDEN_SIZE];
    int32_t hidden_bias[NNUE_HIDDEN_SIZE];
    int8_t output_weights[NNUE_HIDDEN_SIZE];
    int32_t output_bias;
    int16_t accumulator_stack[NNUE_STACK_SIZE + 1][2][NNUE_ACCUMULATOR_SIZE];
} nnue_state_t;

static nnue_state_t nnue_state;

static int sq64_for_perspective(unsigned sq, int perspective_color) {
    int row = (int)INDEX2ROW(sq);
    int col = (int)INDEX2COLUMN(sq);

    if (perspective_color == BLACK)
        row = 7 - row;

    return row * 8 + col;
}

static int feature_piece_index(unsigned piece, int piece_color, int perspective_color) {
    int own = (piece_color == perspective_color);

    switch (piece) {
    case PAWN:
        return own ? 0 : 5;
    case KNIGHT:
        return own ? 1 : 6;
    case BISHOP:
        return own ? 2 : 7;
    case ROOK:
        return own ? 3 : 8;
    case QUEEN:
        return own ? 4 : 9;
    default:
        return -1;
    }
}

static int feature_index(int perspective_color, unsigned piece, int piece_color,
                         unsigned king_sq, unsigned piece_sq) {
    int pidx = feature_piece_index(piece, piece_color, perspective_color);
    int king64, piece64;

    if (pidx < 0)
        return -1;

    king64 = sq64_for_perspective(king_sq, perspective_color);
    piece64 = sq64_for_perspective(piece_sq, perspective_color);
    return (king64 * 10 + pidx) * 64 + piece64;
}

static unsigned king_square_for_perspective(int perspective_color) {
    return (perspective_color == WHITE) ? w_king : b_king;
}

static void accumulator_add_sub(int16_t *dst, const int16_t *weights, int sign) {
    int i;

    for (i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
        dst[i] = (int16_t)(dst[i] + sign * weights[i]);
}

static void refresh_perspective(int stack_index, int perspective_index, int perspective_color) {
    int16_t *acc = nnue_state.accumulator_stack[stack_index][perspective_index];
    unsigned i;
    unsigned king_sq = king_square_for_perspective(perspective_color);

    memcpy(acc, nnue_state.ft_bias, sizeof(nnue_state.ft_bias));

    for (i = 0; i < w_pieces.count; i++) {
        unsigned sq = w_pieces.index[i];
        unsigned piece = pieces[sq];
        int findex;

        if (piece == EMPTY || piece == KING)
            continue;

        findex = feature_index(perspective_color, piece, WHITE, king_sq, sq);
        if (findex >= 0)
            accumulator_add_sub(acc, &nnue_state.ft_weights[findex * NNUE_ACCUMULATOR_SIZE], +1);
    }

    for (i = 0; i < b_pieces.count; i++) {
        unsigned sq = b_pieces.index[i];
        unsigned piece = pieces[sq];
        int findex;

        if (piece == EMPTY || piece == KING)
            continue;

        findex = feature_index(perspective_color, piece, BLACK, king_sq, sq);
        if (findex >= 0)
            accumulator_add_sub(acc, &nnue_state.ft_weights[findex * NNUE_ACCUMULATOR_SIZE], +1);
    }
}

static void refresh_both(int stack_index) {
    refresh_perspective(stack_index, 0, WHITE);
    refresh_perspective(stack_index, 1, BLACK);
}

static void apply_piece_delta(int stack_index, int perspective_index, int perspective_color,
                              unsigned piece, int piece_color,
                              unsigned from_sq, unsigned to_sq) {
    int findex;
    int16_t *acc = nnue_state.accumulator_stack[stack_index][perspective_index];
    unsigned king_sq = king_square_for_perspective(perspective_color);

    if (piece == EMPTY || piece == KING)
        return;

    if (from_sq != OFF) {
        findex = feature_index(perspective_color, piece, piece_color, king_sq, from_sq);
        if (findex >= 0)
            accumulator_add_sub(acc, &nnue_state.ft_weights[findex * NNUE_ACCUMULATOR_SIZE], -1);
    }

    if (to_sq != OFF) {
        findex = feature_index(perspective_color, piece, piece_color, king_sq, to_sq);
        if (findex >= 0)
            accumulator_add_sub(acc, &nnue_state.ft_weights[findex * NNUE_ACCUMULATOR_SIZE], +1);
    }
}

static void apply_piece_delta_both(int stack_index, unsigned piece, int piece_color,
                                   unsigned from_sq, unsigned to_sq) {
    apply_piece_delta(stack_index, 0, WHITE, piece, piece_color, from_sq, to_sq);
    apply_piece_delta(stack_index, 1, BLACK, piece, piece_color, from_sq, to_sq);
}

int nnue_load(const char *path) {
    FILE *fp;
    nnue_header header;

    fp = fopen(path, "rb");
    if (!fp)
        return 0;

    memset(&header, 0, sizeof(header));
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }

    if (memcmp(header.magic, NNUE_MAGIC, 8) != 0 ||
        header.version != NNUE_VERSION ||
        header.ft_inputs != NNUE_FT_INPUTS ||
        header.accumulator_size != NNUE_ACCUMULATOR_SIZE ||
        header.hidden_size != NNUE_HIDDEN_SIZE) {
        fclose(fp);
        return 0;
    }

    free(nnue_state.ft_weights);
    nnue_state.ft_weights = (int16_t *)malloc(sizeof(int16_t) * NNUE_FT_INPUTS * NNUE_ACCUMULATOR_SIZE);
    if (!nnue_state.ft_weights) {
        fclose(fp);
        return 0;
    }

    if (fread(nnue_state.ft_bias, sizeof(int16_t), NNUE_ACCUMULATOR_SIZE, fp) != NNUE_ACCUMULATOR_SIZE ||
        fread(nnue_state.ft_weights, sizeof(int16_t), NNUE_FT_INPUTS * NNUE_ACCUMULATOR_SIZE, fp) !=
            (size_t)(NNUE_FT_INPUTS * NNUE_ACCUMULATOR_SIZE) ||
        fread(nnue_state.hidden_bias, sizeof(int32_t), NNUE_HIDDEN_SIZE, fp) != NNUE_HIDDEN_SIZE ||
        fread(nnue_state.hidden_weights, sizeof(int8_t),
              NNUE_ACCUMULATOR_SIZE * 2 * NNUE_HIDDEN_SIZE, fp) !=
            (size_t)(NNUE_ACCUMULATOR_SIZE * 2 * NNUE_HIDDEN_SIZE) ||
        fread(&nnue_state.output_bias, sizeof(int32_t), 1, fp) != 1 ||
        fread(nnue_state.output_weights, sizeof(int8_t), NNUE_HIDDEN_SIZE, fp) != NNUE_HIDDEN_SIZE) {
        fclose(fp);
        nnue_unload();
        return 0;
    }

    fclose(fp);

    nnue_state.loaded = 1;
    nnue_state.enabled = 1;
    nnue_state.output_scale = header.output_scale > 0 ? header.output_scale : 1;
    strncpy(nnue_state.path, path, sizeof(nnue_state.path) - 1);
    nnue_state.path[sizeof(nnue_state.path) - 1] = '\0';
    nnue_refresh();
    return 1;
}

void nnue_unload(void) {
    free(nnue_state.ft_weights);
    memset(&nnue_state, 0, sizeof(nnue_state));
}

void nnue_set_enabled(int enabled) {
    nnue_state.enabled = enabled ? 1 : 0;
}

int nnue_is_enabled(void) {
    return nnue_state.enabled;
}

int nnue_is_loaded(void) {
    return nnue_state.loaded;
}

int nnue_can_evaluate(void) {
    return nnue_state.loaded && nnue_state.enabled;
}

const char *nnue_loaded_path(void) {
    return nnue_state.path[0] ? nnue_state.path : NULL;
}

void nnue_reset_state(void) {
    memset(nnue_state.accumulator_stack, 0, sizeof(nnue_state.accumulator_stack));
}

void nnue_refresh(void) {
    if (!nnue_can_evaluate())
        return;

    if (total_history > NNUE_STACK_SIZE)
        return;

    refresh_both((int)total_history);
}

void nnue_apply_move(unsigned move, unsigned moved_piece, unsigned captured_piece,
                     unsigned previous_ep, int mover_color) {
    int child_index;

    if (!nnue_can_evaluate())
        return;

    if (total_history > NNUE_STACK_SIZE)
        return;

    child_index = (int)total_history;
    memcpy(nnue_state.accumulator_stack[child_index],
           nnue_state.accumulator_stack[child_index - 1],
           sizeof(nnue_state.accumulator_stack[child_index]));

    if ((move & BITS_CASTLE) || moved_piece == KING) {
        refresh_both(child_index);
        return;
    }

    if (captured_piece != EMPTY) {
        unsigned capture_sq = MOVE2TO(move);
        if (move & BITS_ENPASSANT)
            capture_sq = previous_ep - mover_color * 16;
        apply_piece_delta_both(child_index, captured_piece, -mover_color, capture_sq, OFF);
    }

    if (move & BITS_PROMOTE) {
        apply_piece_delta_both(child_index, moved_piece, mover_color, MOVE2FROM(move), OFF);
        apply_piece_delta_both(child_index, MOVE2PROMOTE(move), mover_color, OFF, MOVE2TO(move));
        return;
    }

    apply_piece_delta_both(child_index, moved_piece, mover_color, MOVE2FROM(move), MOVE2TO(move));
}

void nnue_push_null(void) {
    if (!nnue_can_evaluate())
        return;

    if (total_history > NNUE_STACK_SIZE)
        return;

    memcpy(nnue_state.accumulator_stack[total_history],
           nnue_state.accumulator_stack[total_history - 1],
           sizeof(nnue_state.accumulator_stack[total_history]));
}

int nnue_evaluate(void) {
    int8_t input[NNUE_ACCUMULATOR_SIZE * 2];
    int8_t hidden[NNUE_HIDDEN_SIZE];
    const int16_t *stm_acc;
    const int16_t *nstm_acc;
    int32_t out;
    int i, j;

    if (!nnue_can_evaluate())
        return 0;

    stm_acc = (turn == WHITE)
        ? nnue_state.accumulator_stack[total_history][0]
        : nnue_state.accumulator_stack[total_history][1];
    nstm_acc = (turn == WHITE)
        ? nnue_state.accumulator_stack[total_history][1]
        : nnue_state.accumulator_stack[total_history][0];

    for (i = 0; i < NNUE_ACCUMULATOR_SIZE; i++) {
        int v = stm_acc[i];
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        input[i] = (int8_t)v;
    }

    for (i = 0; i < NNUE_ACCUMULATOR_SIZE; i++) {
        int v = nstm_acc[i];
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        input[NNUE_ACCUMULATOR_SIZE + i] = (int8_t)v;
    }

    for (i = 0; i < NNUE_HIDDEN_SIZE; i++) {
        int32_t sum = nnue_state.hidden_bias[i];
        for (j = 0; j < NNUE_ACCUMULATOR_SIZE * 2; j++)
            sum += (int32_t)input[j] *
                   (int32_t)nnue_state.hidden_weights[j * NNUE_HIDDEN_SIZE + i];
        if (sum < 0) sum = 0;
        if (sum > 127) sum = 127;
        hidden[i] = (int8_t)sum;
    }

    out = nnue_state.output_bias;
    for (i = 0; i < NNUE_HIDDEN_SIZE; i++)
        out += (int32_t)hidden[i] * (int32_t)nnue_state.output_weights[i];

    return (int)(out / nnue_state.output_scale);
}
