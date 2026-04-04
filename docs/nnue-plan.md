# NNUE Implementation Plan

Self-contained NNUE for Cerulean — no external libraries, self-trained from scratch on commodity hardware (M5 Mac, 32GB RAM). Portable C99: no platform-specific intrinsics, relies on `-O3` auto-vectorization for SIMD on both ARM and x86.

## Architecture: HalfKP(friend)

```
Feature Transformer:  40,960 inputs -> 128 (per perspective, sparse)
  inputs = 64 king_sq x 10 piece_types x 64 piece_sq
  piece_types = {own_pawn, own_knight, own_bishop, own_rook, own_queen,
                 enemy_pawn, enemy_knight, enemy_bishop, enemy_rook, enemy_queen}

Output Network:  256 -> 32 -> 1
  input = concat(stm_accumulator[128], nstm_accumulator[128])
  activations = ClippedReLU(0, 127)
  output = centipawns
```

### Why 128 hidden, not 256?

| Dimension | Net size | FT params | NPS impact | Training data needed | Strength ceiling |
|-----------|----------|-----------|------------|---------------------|-----------------|
| **64** | ~5 MB | 2.6M | ~1.5x slower than HCE | ~200K positions | ~+100 ELO |
| **128** | ~10 MB | 5.2M | ~2x slower than HCE | ~500K positions | ~+200-300 ELO |
| **256** | ~21 MB | 10.5M | ~3-5x slower than HCE | ~2M+ positions | ~+300-400 ELO |
| **512** | ~42 MB | 21M | ~6-10x slower than HCE | ~10M+ positions | ~+400+ ELO |

**128 is the right starting point because:**
- Cerulean's self-play data is limited (~500K-1M positions per round). A 256-wide net would underfit with that data — it needs millions of high-quality positions to outperform 128.
- Higher NPS partially compensates for less expressive eval. At Cerulean's depth range (8-12 ply), NPS matters a lot.
- Faster training iterations = more experiment cycles = faster convergence on good hyperparameters.
- Easy to scale up later: retrain with 256 once the data pipeline is mature and producing millions of positions.

The output network is 256->32->1 (single hidden layer) instead of 512->32->32->1. Two hidden layers add marginal accuracy but measurable inference cost. One layer is standard for engines in this strength range.

### Quantization

| Layer | Weights | Biases | Accumulator |
|-------|---------|--------|-------------|
| Feature Transformer | int16 | int16 | int16 |
| Hidden (256->32) | int8 | int32 | int32 |
| Output (32->1) | int8 | int32 | int32 |

Net file size: 40960 x 128 x 2 + 128 x 2 + 256 x 32 + 32 x 4 + 32 + 4 = **~10.3 MB**

**Square mapping:** 0x88 to 64-square via `(idx >> 4) * 8 + (idx & 7)`. Black perspective flips ranks.

## NNUE vs HCE: Full Replacement

**NNUE fully replaces HCE for game play.** The HCE code stays in the codebase but becomes secondary:

| Context | Evaluator |
|---------|-----------|
| Normal search (net loaded) | NNUE only |
| No net file available | HCE fallback |
| `evaluate` command (human display) | HCE (readable breakdown) |
| Search pruning margins | Retuned for NNUE scale |
| `nnue off` command | HCE for A/B testing |

The HCE is not blended with NNUE — hybrid approaches add complexity and the NNUE should surpass HCE entirely once trained. Pruning thresholds (razoring margin 350cp, futility 110cp, delta 350cp) will need retuning since NNUE scores may have different scale characteristics. The training loss function uses a sigmoid scaling factor (SCALE=400) that maps NNUE output to win probability, keeping centipawn interpretation roughly consistent.

## Incremental Accumulator Updates

The accumulator is a 128-element int16 vector per perspective (white/black), stored per ply in a stack of size MAX_PLY.

| Move type | Update |
|-----------|--------|
| Quiet move | Sub old feature, add new feature (1 change per perspective) |
| Capture | Sub captured piece feature + sub/add moved piece (2 changes) |
| Promotion | Sub pawn feature, add promoted piece feature |
| King move | Full refresh (all features are king-relative) |
| Castling | Full refresh both perspectives |
| En passant | Sub captured pawn + sub/add moved pawn |
| Unmake | Decrement ply index (previous accumulator preserved) |

On `board_add()`: push accumulator, apply delta. On `board_subtract()`: pop accumulator.

## SIMD Strategy: Portable Auto-Vectorization

No platform-specific intrinsics. Instead, write loops that `-O3` auto-vectorizes on both ARM (NEON) and x86 (SSE/AVX):

```c
// This pattern auto-vectorizes on all platforms with -O3
void accumulator_add(int16_t *acc, const int16_t *weights, int n) {
    for (int i = 0; i < n; i++) {
        acc[i] += weights[i];
    }
}

void clipped_relu(const int16_t *input, int8_t *output, int n) {
    for (int i = 0; i < n; i++) {
        int16_t v = input[i];
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        output[i] = (int8_t)v;
    }
}
```

**Why this works well enough:**
- clang/gcc `-O3` auto-vectorizes simple array loops with no aliasing ambiguity
- Use `restrict` pointers where applicable to help the optimizer
- Accumulator dimension 128 = 8 x int16 NEON vectors = 16 x int8 SSE vectors — clean multiples
- The dense layers (256x32, 32x1) are small enough that auto-vectorized loops are near-optimal
- Measured overhead vs hand-written intrinsics: typically <10% for these loop shapes

**Compiler hints to assist auto-vectorization:**
- Arrays aligned to 64 bytes: `_Alignas(64) int16_t acc[128]`
- `restrict` on pointer params to eliminate alias analysis
- Loop trip counts that are compile-time constants or multiples of vector width

## Training Data Pipeline

### Seed data (~3,100 positions)

Parse FENs from existing suite files and label each with a depth-7 search score:

- `suites/perftsuite.epd` — 764 positions (broad diversity)
- `suites/*.epd` (arasan12, bt2630, ecmgcp, eet, lapuce2, pet, sbd, wac) — ~1,045 tactical/strategic positions
- `suites/epd/STS1-STS13.epd` — ~1,300 strategic theme positions

### Self-play data

- Randomize first 8 half-moves for opening diversity
- Play to completion at depth 6-8
- Record positions after move 8 with search score + game result (WDL)
- Adjudicate: |eval| > 1000cp for 5 consecutive moves = decisive, 100+ moves = draw
- Filter: skip positions in check, |score| > 3000, or fewer than 4 pieces

### Binary format (40 bytes per entry)

```c
typedef struct __attribute__((packed)) {
    uint8_t  pieces[32];   // Packed board: 2 pieces per byte (4 bits each)
    uint8_t  stm;          // 0=white, 1=black
    uint8_t  castling;     // Castling rights bitmask
    uint8_t  ep_square;    // En passant square (64=none)
    uint8_t  result;       // 0=black wins, 1=draw, 2=white wins, 3=unknown
    int16_t  score;        // Search score in centipawns
    uint16_t ply_count;    // Game ply
} TrainingEntry;           // 40 bytes
```

## Time Estimates

### Data generation

| Step | Work | Time estimate | Basis |
|------|------|---------------|-------|
| Seed labeling | 3,100 positions x depth 7 (~50ms each) | **~2.5 min** | 50ms/position at depth 7 |
| Gen 1 self-play | 10K games x ~80 moves x ~10ms/move (depth 6) | **~2.2 hrs** | Conservative; depth 6 with HCE |
| Gen 2 self-play | 20K games x ~80 moves x ~15ms/move (depth 7) | **~6.5 hrs** | Deeper search, NNUE eval slightly slower |

### Training

| Step | Work | Time estimate | Basis |
|------|------|---------------|-------|
| Bootstrap (seed data) | 3.1K positions x 200 epochs, batch 1024 | **~3 min** | Tiny dataset, fast epochs |
| Gen 1 training | 500K positions x 100 epochs, batch 16384 | **~18 min** | ~11s/epoch for 500K samples |
| Gen 2 training | 1.5M positions x 100 epochs, batch 16384 | **~50 min** | ~30s/epoch for 1.5M samples |

### End-to-end per round

| Round | Total time | Cumulative |
|-------|-----------|------------|
| Bootstrap (seed only) | ~5 min | ~5 min |
| Round 1 (gen1 + train) | ~2.5 hrs | ~2.5 hrs |
| Round 2 (gen2 + train) | ~7 hrs | ~10 hrs |
| Round 3+ | ~7 hrs each | ~17 hrs |

**First playable NNUE: ~2.5 hours. First well-trained net: ~10 hours over 2 rounds.**

## Logging, Resumability, and Idempotency

### Datagen: Resumable

Data generation is the slowest step. It must be resumable.

```
$ make datagen GAMES=10000 OUTPUT=data/gen1.bin
[datagen] Loading seed positions from suites/...
[datagen] Parsed 3100 seed positions
[datagen] Labeling seed positions at depth 7...
[datagen]   500/3100 positions labeled (16.1%) - 42s elapsed, ~220s remaining
[datagen]   1000/3100 positions labeled (32.3%) - 85s elapsed, ~178s remaining
[datagen]   ...
[datagen] Seed labeling complete: 3100 positions in 155s
[datagen] Starting self-play generation: 10000 games at depth 6
[datagen]   Resuming from game 4231 (found data/gen1.bin.progress)
[datagen]   4300/10000 games (43.0%) - 1847 positions/game avg - ETA 1h12m
[datagen]   4400/10000 games (44.0%) - 1851 positions/game avg - ETA 1h10m
[datagen]   ...
[datagen] Complete: 10000 games, 523847 positions written to data/gen1.bin
```

**Resumability mechanism:**
- Write a `.progress` sidecar file after every N games (e.g., every 100): `{ "games_completed": 4231, "positions_written": 247812, "bytes_written": 9912480 }`
- On startup, if `.progress` exists, seek to `bytes_written` in output file and resume from `games_completed + 1`
- Seed labeling writes to a separate `.seed.bin` file that is generated once (idempotent — skip if file exists and size matches expected count)

### Training: Checkpointed

```
$ make train DATA=data/gen1.bin NET=nets/gen1.nnue
[train] Loading training data: data/gen1.bin (523847 positions, 20.4 MB)
[train] Network: 40960 -> 128 -> 32 -> 1 (5,251,233 parameters)
[train] Optimizer: Adam (lr=0.001, batch=16384)
[train] Epoch   1/100  loss=0.0832  lr=0.001000  11.2s  (ETA: 18m40s)
[train] Epoch   2/100  loss=0.0714  lr=0.001000  11.1s  (ETA: 18m09s)
[train]   ...
[train] Epoch  50/100  loss=0.0312  lr=0.000500  11.3s  (ETA: 9m25s)
[train]   Checkpoint saved: nets/gen1.checkpoint.50
[train]   ...
[train] Epoch 100/100  loss=0.0289  lr=0.000250  11.2s
[train] Training complete. Best loss: 0.0287 at epoch 97
[train] Quantizing float net -> int16/int8...
[train] Wrote nets/gen1.nnue (10.3 MB)
[train] Validation: STS-fast score = 847/1300 (baseline HCE: 792/1300, delta: +55)
```

**Checkpoint mechanism:**
- Save float weights + Adam state every 10 epochs to `<net>.checkpoint.<epoch>`
- On startup, if checkpoint exists, offer to resume: loads weights + optimizer state, continues from that epoch
- Keep only last 2 checkpoints to avoid disk bloat
- Final `.nnue` file is the quantized output — deterministic from the float checkpoint (idempotent)

### STS evaluation: Idempotent

```
$ make sts-eval NET=nets/gen1.nnue
[sts-eval] Loading net: nets/gen1.nnue (10.3 MB)
[sts-eval] Running STS at 100ms/move (1300 positions)
[sts-eval]   STS1  (Undermining):     82/100
[sts-eval]   STS2  (Open Files):      71/100
[sts-eval]   ...
[sts-eval]   STS13 (Pawn Play):       68/100
[sts-eval] Total: 847/1300
[sts-eval] Wrote results to nets/gen1.sts-results.txt
```

Pure read-only evaluation — always idempotent, re-runnable.

### Full pipeline: `make nnue-cycle`

```makefile
nnue-cycle:
	@echo "[cycle] === NNUE Training Cycle ==="
	@echo "[cycle] Step 1/3: Data generation"
	$(MAKE) datagen GAMES=$(GAMES) OUTPUT=$(DATA)
	@echo "[cycle] Step 2/3: Training"
	$(MAKE) train DATA=$(DATA) NET=$(NET) EPOCHS=$(EPOCHS)
	@echo "[cycle] Step 3/3: Evaluation"
	$(MAKE) sts-eval NET=$(NET)
	@echo "[cycle] === Cycle complete ==="
```

Each step checks for prior completion before running. The full cycle can be interrupted and re-run safely.

## Training

### Network (float32 for training)

```
ft_weights[40960][128]   ~20 MB
ft_biases[128]
l1_weights[256][32]
l1_biases[32]
out_weights[32]
out_bias

Total with Adam moments: ~62 MB
```

### Loss function

```
loss = lambda * MSE(sigmoid(pred/400), sigmoid(target_score/400))
     + (1-lambda) * MSE(sigmoid(pred/400), result)
```

Lambda transitions from 1.0 (pure score fitting) during bootstrap to 0.75 (result blending) during self-play iterations.

### Optimizer: Adam (implemented from scratch)

- lr=0.001 with cosine decay to 0.00025 over training
- beta1=0.9, beta2=0.999, eps=1e-8
- Batch size: 16,384 (1,024 for bootstrap on small seed data)
- Sparse FT updates: only touch ~30 active features per sample
- One epoch of 500K positions at batch 16384: ~11 seconds

### Training schedule

1. **Bootstrap:** Train on seed EPD data labeled with HCE scores. Lambda=1.0, 200 epochs (~3 min). Gets NNUE to approximate HCE.
2. **Gen 1:** Self-play 10K games with bootstrap net. Train on combined data. Lambda=0.9, 100 epochs (~18 min).
3. **Gen 2+:** Self-play 20K games with latest net. Train on accumulated data. Lambda=0.75, 100 epochs. Repeat until STS/ELO plateaus.
4. **Quantize** float net to int16/int8 after each round. Save as `.nnue` file.

## New Files

| File | Est. lines | Purpose |
|------|-----------|---------|
| `nnue.h` | 80 | Inference types, accumulator struct, API |
| `nnue.c` | 600 | Quantized forward pass, incremental updates, net file I/O |
| `nnue_train.h` | 60 | Training network struct, API |
| `nnue_train.c` | 1,200 | Float forward/backward, Adam, sparse FT gradients, quantization export |
| `datagen.h` | 30 | Data generation API |
| `datagen.c` | 500 | EPD parsing, self-play engine, binary format, progress tracking |

## Changes to Existing Files

| File | Change |
|------|--------|
| `evaluate.c` | Dispatch to `nnue_evaluate()` when net loaded; keep HCE as fallback + display mode (~20 lines) |
| `search.c` | `nnue_push()`/`nnue_update()` around `board_add()`/`board_subtract()` in `search()` and `qsearch()`. Refresh at `iterative_deepening()` root. Null-move handling. (~80 lines) |
| `xboard.c` | Commands: `nnue <path>`, `nnue off`, `datagen`, auto-load at startup (~50 lines) |
| `main.c` | CLI dispatch for training mode (~15 lines) |
| `Makefile` | New source files, `cerulean-train` target, `datagen`/`train`/`sts-eval`/`nnue-cycle` targets (~30 lines) |

## Build Targets

```makefile
make                    # Engine with NNUE inference (loads .nnue file)
make cerulean-train     # Training binary (includes datagen + trainer)
make datagen            # Generate training data (resumable)
make train              # Run training (checkpointed)
make sts-eval           # Evaluate net with STS (idempotent)
make nnue-cycle         # Full pipeline: datagen -> train -> evaluate
```

## Evaluation Metrics

1. **STS:** `make sts-eval` before/after each training round. Target: +50 points over HCE baseline.
2. **Search test:** `make searchtest` — same or more positions solved.
3. **Self-play ELO:** Head-to-head with `cutechess-cli` (optional external tool), 500+ games.
4. **NPS benchmark:** `go depth 10` on startpos, compare HCE vs NNUE.

## Debugging & Correctness

- **Assert mode:** Verify incremental accumulator matches full refresh after every move during development.
- **Perft:** `make perfttest` must remain unchanged (eval doesn't affect movegen).
- **Reproducibility:** Fixed seed for MT64 in datagen for reproducible training sets.
- **Gradient checking:** Numerical gradient verification for the first few training batches.

## Implementation Order

1. `nnue.h/nnue.c` — inference with random weights, file I/O
2. Search integration — accumulator push/pop/update in `search.c`
3. `datagen.c` — EPD seed parser + self-play generator with progress logging
4. `nnue_train.c` — training loop with Adam optimizer and checkpointing
5. Bootstrap training on seed data
6. Self-play iteration loop (generate -> train -> evaluate -> repeat)
7. Profile and tune for NPS
8. Retune search pruning margins for NNUE eval scale
