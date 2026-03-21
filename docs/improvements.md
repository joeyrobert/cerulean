# Cerulean Improvement Plan

Three targeted improvements expected to yield ~+85–145 ELO combined, applicable to both
**cerulean** (C, 0x88 mailbox) and **ceruleanjs** (JavaScript, 0x88 mailbox).

Estimated current strength: cerulean ~1755 CCRL, ceruleanjs ~1337 CCRL (March 2026 tournament).

---

## 1. King Safety (~+50–80 ELO)

### Problem

Both engines currently evaluate king safety only by counting missing pawn shield pawns
(`KING_SHIELD_PENALTY * (3 - shieldCount)`). This misses the most important factor: how
many enemy pieces are attacking squares near the king.

A king behind three pawns but attacked by queen + two rooks is in grave danger. A king
with a missing pawn but no attackers is fine. The current evaluation can't distinguish these.

### Approach

The standard technique is an **attack unit table**. For each enemy piece that attacks a
square adjacent to the king, accumulate weighted "attack units":

| Attacker | Units |
|---|---|
| Knight | 2 |
| Bishop | 2 |
| Rook | 3 |
| Queen | 5 |

Then apply a non-linear penalty from a lookup table indexed by total units. The penalty
is zero for 0–1 units, grows slowly to ~2 units, then sharply above 3+ (a king under
coordinated attack is in exponentially more danger).

Also add:
- **Open/half-open file toward king**: +penalty per file with no friendly pawn between
  the king and the enemy rook/queen
- **No attacking pieces discount**: If the attacker count is 0 or 1, suppress the penalty
  entirely (no attack = no danger regardless of structure)
- **Scale with game phase**: King safety matters far less in the endgame (fewer attackers).
  Multiply the entire king safety score by `gamePhase`.

### Implementation Notes

**cerulean (C):**

In `evaluate.c`, the `king()` function (or equivalent loop) already iterates over pieces.
Add a pass over enemy pieces checking if they attack any of the 8 squares adjacent to the
king. Reuse the existing `is_attacked()` infrastructure or add a dedicated attack-counting
helper to avoid redundant work.

```c
// Pseudocode — in evaluate.c
int king_attack_units = 0;
int king_attackers = 0;
for each enemy piece p:
    if p attacks any square in king_zone[our_king]:
        king_attack_units += ATTACK_WEIGHT[piece_type(p)]
        king_attackers++

// Only penalize if 2+ pieces are coordinating
if king_attackers >= 2:
    score -= SAFETY_TABLE[min(king_attack_units, 99)] * game_phase
```

The `SAFETY_TABLE` is a 100-entry array; values from 0 to ~500cp. Stockfish's original
hand-tuned table is a good starting point (widely published).

**ceruleanjs (JS):**

The `king()` method in `evaluate.js` already takes `gamePhase`. Add the attack-unit
accumulation loop there. `board.deltaMoveCount` and `board.isAttacked` already exist —
write a new `kingZoneAttackUnits(board, kingIndex, attackerTurn)` helper that reuses the
existing delta tables.

### Interaction with Existing Code

- Keep the pawn shield logic — it's complementary, not redundant.
- The open-file penalty toward king partially overlaps with the existing
  `ROOK_OPEN_FILE_VS_KING_BONUS` in ceruleanjs — consolidate or remove the old one to
  avoid double-counting.

---

## 2. Tapered Evaluation (~+20–40 ELO)

### Problem

Phase interpolation is currently only applied to the **king PST**. All other evaluation
terms (mobility bonuses, rook bonuses, passed pawn bonuses, piece-specific bonuses) use
fixed values regardless of game phase. This is inaccurate: a rook on an open file is worth
more in the endgame; a passed pawn is worth more; a knight outpost is worth less.

### Approach

Apply the existing `gamePhase` interpolation (`interpolate(early, late, phase)`) to every
evaluation term that has meaningfully different opening vs endgame value. At minimum:

| Term | Opening | Endgame | Rationale |
|---|---|---|---|
| Mobility bonus (all pieces) | ×1.0 | ×0.7 | Less decisive in endgame |
| Rook open file bonus | ×0.8 | ×1.3 | More valuable with fewer pieces |
| Passed pawn bonus | ×0.6 | ×1.6 | Decisive in endgame, irrelevant early |
| Knight outpost bonus | ×1.3 | ×0.7 | Knights weaker in open endgames |
| Bishop pair bonus | ×0.8 | ×1.4 | Opens up as pawns trade off |
| King activity bonus | already tapered | — | already correct |
| King shield penalty | already tapered | — | already correct |

The multipliers above are starting points — they should be tuned (see §4).

### Implementation Notes

**cerulean (C):**

The `game_phase` float (0.0 = endgame, 1.0 = opening) is computed once per `evaluate()`
call. Pass it into the per-piece evaluation helpers or store it as a local variable in
scope. Replace fixed bonus additions with:

```c
// Before:
score += ROOK_OPEN_FILE_BONUS;  // always +15

// After:
score += interpolate(12, 20, game_phase);  // +12 opening, +20 endgame
```

Add an `interpolate(int early, int late, float phase)` function matching the ceruleanjs
version if not already present.

**ceruleanjs (JS):**

`this.interpolate()` already exists. Replace fixed bonus references in `evaluate()` with
`this.interpolate(earlyVal, lateVal, gamePhase)`. The `gamePhase` variable is already in
scope throughout the evaluation loop.

### What Not to Taper

- Material values: keep fixed (material is material regardless of phase)
- Tempo bonus: keep fixed
- Pawn structure penalties (doubled, isolated, backward): small benefit to tapered, low priority

---

## 3. Singular Extensions (~+20–40 ELO)

### Problem

The engine extends on checks but has no other extension mechanism. Singular extensions
identify moves that are "forced" — where one move is so much better than all alternatives
that the position is effectively a one-reply position. Extending these avoids missing
tactical shots that occur just past the normal horizon.

### Approach

At an interior node where:
1. Depth ≥ 8
2. There is a TT move with `score >= beta - singularMargin` and type EXACT or LOWER
3. Not already in an extension

Perform a **reduced-depth search** with a null window `[beta - singularMargin - 1, beta - singularMargin]`
excluding the TT move. If this search fails low (no move beats the threshold), the TT
move is singular — extend it by 1 ply.

```
singularMargin = 2 * depth  // typical value, tune between 1.5*depth and 3*depth
reducedDepth   = (depth - 1) / 2
```

### Implementation Notes

**cerulean (C):**

In `search.c`, in the main alpha-beta loop where the TT move is identified and before the
move loop begins:

```c
// Singular extension check
int extension = 0;
if (depth >= 8
    && tt_move != NO_MOVE
    && tt_score >= beta - 2 * depth
    && tt_type != UPPER_BOUND
    && !in_singular_search) {

    int s_beta  = tt_score - 2 * depth;
    int s_depth = (depth - 1) / 2;
    // Search all moves EXCEPT tt_move at s_depth with window [s_beta-1, s_beta]
    int s_score = singular_search(board, s_beta - 1, s_beta, s_depth);
    if (s_score < s_beta) {
        extension = 1;  // TT move is singular, extend when we search it
    }
}
```

Add an `in_singular_search` flag (passed through the call stack or stored in a thread-local)
to prevent recursive singular searches.

**ceruleanjs (JS):**

In `search.js`, same logic. Pass `inSingularSearch = false` as a default parameter.
The reduced search reuses the existing `alphaBeta()` function with an extra exclusion
parameter: skip `move === ttMove` during move generation for the singular probe.

### Interaction with Existing Extensions

- Check extension: keep as-is, apply after singular check (can stack to +2 in rare cases,
  cap at +2 total to avoid search explosion)
- LMR: do not reduce the singular move (it already gets an extension, reducing would
  cancel it out)

---

## 4. Suggested Order of Implementation

```
1. Tapered eval          — lowest risk, touches only evaluate.c / evaluate.js
                           Can verify correctness: eval(pos) should be identical to
                           current on pure middlegame positions (phase=1.0 with identity
                           multipliers), and differ predictably in endgames.

2. King safety           — medium risk, adds a new evaluation component
                           Verify by constructing positions where a king under attack
                           is correctly penalized (e.g., open file toward castled king
                           with enemy rook present).

3. Singular extensions   — highest risk, touches search logic
                           Verify with perft (node counts must be unchanged on a fixed
                           set of positions — singular extension only fires in real
                           search, not perft). Run STS suite before/after.
```

Run the cutechess-cli calibration tournament (see `cutechess/tournament.sh`) after each
step to measure actual ELO gain before proceeding.

---

## 5. Applying to ceruleanjs

ceruleanjs shares the same 0x88 board representation and evaluation structure as cerulean.
All three improvements translate directly. Key differences to account for:

- `evaluate.js` uses `this.interpolate()` — no new helper needed for §2
- `search.js` uses a recursive closure style — pass `inSingularSearch` as a parameter
- JS lacks `int` overflow guards — use `Math.floor()` for integer division in §3
- The eval params are in `eval_params.json` — add `SAFETY_TABLE`, `EARLY_*` / `LATE_*`
  variants there to keep constants configurable without code changes

Once cerulean is tuned and confirmed improved, port the final constant values to
`eval_params.json` for ceruleanjs.
