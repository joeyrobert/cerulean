# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Cerulean is a competitive chess engine written in C (C99), ported from ceruleanjs. It uses 0x88 mailbox board representation and supports the XBoard/CECP protocol. Estimated strength: ~1755 CCRL.

## Build Commands

```bash
make              # Build the cerulean binary (clang, -O3)
make clean        # Remove object files and binary
make test         # Run perft + search test suites
make perfttest    # Run perft suite only (move generation validation)
make searchtest   # Run search quality test suite (8 EPD sets)
make sts          # Run Strategic Test Suite at 1s/move
make sts-fast     # Run STS at 100ms/move
```

The engine reads commands from stdin. To run interactively: `./cerulean` then type commands. To run a single command: `echo "perfttest" | ./cerulean`.

Useful interactive commands: `evaluate` (full eval breakdown), `display` (ASCII board), `moves` (legal moves), `setboard <FEN>`, `cachestat` (hash table stats).

## Architecture

**Board representation (`board.c/h`):** 0x88 mailbox (128-element array). Moves are 32-bit unsigned ints encoded as `BITS[31:24] | PROMOTE[23:16] | FROM[15:8] | TO[7:0]`. BITS flags: CAPTURE, CASTLE, EN_PASSANT, PAWN_DOUBLE, PAWN_MOVE, PROMOTE.

**Search (`search.c/h`):** Iterative deepening with aspiration windows, alpha-beta negamax with PVS, LMR (depth>=4, move>5), null-move pruning, razoring, futility pruning, IID, check extensions, quiescence search with delta pruning. Killer moves and relative history heuristic for move ordering.

**Evaluation (`evaluate.c/h`):** Material + piece-square tables with king PST interpolation (opening to endgame). Includes mobility, pawn structure (doubled/isolated/backward/passed/connected), piece-specific bonuses (knight outposts, bishop pair, rook on open files), king safety shield, center control, tempo bonus. Game phase tracked on 24-point system.

**Hash tables (`hash_table.c/h`, `zobrist.c/h`):** Three-table system: transposition table (search), eval cache (25% memory), pawn hash (25% memory). Zobrist hashing with power-of-2 AND-mask indexing. Three bound types: EXACT, ALPHA, BETA.

**Protocol (`xboard.c/h`):** XBoard/CECP protocol handler. Entry point is `xboard_run()` called from `main.c`.

**Piece tracking (`piece_list.c/h`):** Auxiliary data structure for piece location tracking.

## Key Constants

Material values: Pawn=100, Knight=300, Bishop=310, Rook=500, Queen=975. Defined in header files alongside PST arrays.

## Test Suites

EPD test files live in `suites/` (perft, search tests) and `suites/epd/` (STS1-STS13). There are 764 perft positions and 1045 search positions across 8 test sets. One known perft failure exists (en passant edge case inherited from ceruleanjs).

## Planned Improvements

See `docs/improvements.md` for three targeted improvements: king safety evaluation, tapered evaluation, and singular extensions.
