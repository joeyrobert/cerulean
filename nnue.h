#ifndef NNUE_H
#define NNUE_H

#include <stdint.h>

#define NNUE_FT_INPUTS 40960
#define NNUE_ACCUMULATOR_SIZE 128
#define NNUE_HIDDEN_SIZE 32
#define NNUE_STACK_SIZE 2048

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t ft_inputs;
    uint32_t accumulator_size;
    uint32_t hidden_size;
    int32_t output_scale;
    uint32_t reserved[6];
} nnue_header;

int nnue_load(const char *path);
void nnue_unload(void);
void nnue_set_enabled(int enabled);
int nnue_is_enabled(void);
int nnue_is_loaded(void);
int nnue_can_evaluate(void);
const char *nnue_loaded_path(void);

void nnue_reset_state(void);
void nnue_refresh(void);
void nnue_apply_move(unsigned move, unsigned moved_piece, unsigned captured_piece,
                     unsigned previous_ep, int mover_color);
void nnue_push_null(void);
int nnue_evaluate(void);

#endif
