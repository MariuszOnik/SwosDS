# swos-vm-c

Experimental, parallel approach to `../swos-ds`: instead of re-deriving SWOS
match mechanics by reading `swos-port`'s C++ and guessing, this mechanically
ports OpenSWOS's `game/scripts/SwosVm` + `game/scripts/Sim/Port` (C#, MIT
licensed, ~33k lines across 56 files) to portable C, preserving memory
addresses/offsets rather than jumping straight to idiomatic C structs. See
`../DEVLOG.md`, 2026-09-15 entry, and `../audyt-openswos-sim.md` for the
full rationale and tick-order audit this is based on.

`../swos-ds` is **not touched** by this experiment — its own history is
frozen at the `pre-openswos-port-experiment` tag so the two approaches can
be compared without risk to either.

## Why mechanical, not idiomatic

OpenSWOS's `Sim/Port` code is unusually densely cited back to the original
disassembly (`// updatePlayers.cpp:7604-7891`, `// swos.asm:203952`, with
`goto`-labels kept under their original names like `cseg_80E8F`). Porting
address-for-address lets us diff against that citation trail line by line;
switching straight to idiomatic C structs would require re-deriving which
struct field a given offset means *while also* translating language, which
is where hand-ported-from-scratch physics tends to quietly diverge (this is
exactly what happened in `swos-ds` — see `PHYSICS_ANALYSIS.md`). Once a
module's mechanical port passes its tests, it's a candidate for a later,
separate idiomatic-struct pass — not before.

## Layout

- `include/swos_addr.h` — **generated**, do not hand-edit. Every named
  memory offset from `SwosVm.Memory.Addr` (384 constants).
- `include/generated/swos_tables_data.h` — **generated**, do not hand-edit.
  Literal data tables (RNG permutation table, sine/cosine, angle-tangent)
  extracted verbatim rather than retyped, to rule out transcription errors.
- `include/`, `src/` — hand-ported modules (see status below). Each `.c`/`.h`
  carries `SOURCE:` (exact OpenSWOS file/lines) and `FIDELITY:` tags per
  entry.
- `include/generated/swos_anim_streams.h` — **generated**, do not hand-edit.
  174 animation frame-indices arrays from `AnimationTablesData.cs`,
  extracted verbatim (`tools/extract_all_arrays.py`).
- `tools/convert_addr.py`, `tools/extract_table.py`, `tools/extract_all_arrays.py`
  — the generators for the generated headers above. Re-run them if the
  corresponding OpenSWOS source file changes; never edit their output by
  hand.
- `tools/csharp-golden-dump/` — a small .NET console harness that compiles
  OpenSWOS's real `SwosVm`/`SpriteUpdate.cs` source in place and runs it
  (`Memory.Init()`, and representative `SpriteUpdate` scenarios) to produce
  byte-exact reference output (see Status: step 2.5 / step 3 below). Not
  part of the port itself — verification tooling only.
- `tests/*.c` — desktop tests (new code, not ports of anything) that
  cross-check the ported layer against the OpenSWOS source, most notably
  `tests/test_golden_dump.c` and `tests/test_sprite_update_golden.c`
  (byte-exact comparisons against the C# harness's output).

## Fidelity tags

Every ported function/file is tagged, per the plan in DEVLOG.md:

- `VERIFIED_PC` — direct mechanical port, PC-mode behavior confirmed against
  the OpenSWOS source comment or a cited `swos-port` line.
- `AMIGA_VARIANT` — PC path ported; the Amiga-mode alternative (see
  `audyt-openswos-sim.md` Part 0.4, "Krok 0") is not wired in yet.
- `INFERRED` — OpenSWOS's own comments mark this as reconstructed/guessed
  behavior, not read from disassembly.
- `PORT_EXTENSION` — OpenSWOS-only code with no original-SWOS equivalent
  (skip when porting into this repo, or mark clearly if kept for
  plumbing reasons).
- `FIDELITY_DIFFERENCE` — OpenSWOS and `swos-port` disagree on this point.
  Port OpenSWOS's behavior as written regardless; this tag exists to flag
  the disagreement for a later, separate decision — never to justify
  "fixing" it during the port itself.

**Correction (2026-09-15, after step 2's review): this is backwards from how
the experiment actually needs to run.** `../swos-port` (checked out as
reference in the sibling project) is **auxiliary-only** — read it to
understand *why* an OpenSWOS offset/comment looks the way it does, or to
write up a discrepancy note. Never use it to override OpenSWOS's C#: not the
translation, not an offset, not a type, not the order of operations, not the
logic. The point of this experiment is "does OpenSWOS's *actual, working* VM
mechanically port to C and run on DS" — that question only has a clean
answer if `swos-vm-c` first matches OpenSWOS byte-for-byte, including
anywhere OpenSWOS itself might be wrong relative to the original SWOS. If
OpenSWOS and swos-port disagree: port OpenSWOS as written, note the
disagreement (`FIDELITY_DIFFERENCE`), and leave the fix for a later, separate
pass — never fix it silently in the same commit as the port. (Step 2 found
zero real disagreements after full offset verification — see its status
entry below — so this rule hasn't been exercised in anger yet, but it's the
standing rule going forward.)

## Status (2026-09-15)

Step 1 of the porting order (Memory / Flags / Rng / Tables) — done:

| Module | State |
|---|---|
| `swos_addr.h` (`Memory.Addr`) | Done — generated, 384/384 constants |
| `swos_tables_data.h` (`Rng.kRandomTable`, `Tables.k*`) | Done — generated |
| `swos_flags.{h,c}` (`Flags.cs`) | Done — full hand port |
| `swos_rng.{h,c}` (`Rng.cs`) | Done — full hand port (both streams) |
| `swos_memory.{h,c}` Read/Write helpers | Done — full hand port |
| `swos_memory` `Init()` (`Memory.cs:1477-2271`, ~800 lines) | Done — see "Status: step 2.5" below (ported once its `PlayerSprite.Init()`/`AnimationTablesData.Init()`/`TeamData.Init()` dependencies existed, and golden-dump verified). |

`make test` builds and runs the smoke test (desktop-only; see Makefile
comment for why it pins devkitPro's bundled mingw64 gcc as the *host*
compiler — unrelated to the project's actual ARM/BlocksDS target toolchain).

**Step 1 hardening pass (same day, after external review):** the Read/Write
layer had a latent UB bug (see swos_memory.c) and two signatures took `int`
where an unsigned type was actually required; both fixed. `swosRngReseed()`
was re-tagged `PORT_EXTENSION` (not `VERIFIED_PC`) — it's an OpenSWOS policy
with no equivalent in `swos-port`'s `random.cpp`, which never reseeds at
all; see swos_rng.h for the full note. Added RNG golden vectors computed
independently in Python (not by calling this repo's C code) and flag
boundary tests (signed overflow, unsigned carry-out). Added `.gitattributes`
so regenerating the two generated headers doesn't show as line-ending-only
diffs. All Read/Write/View calls are now bounds-checked via `assert()`
(no-op under `NDEBUG`, so a release/DS build pays nothing).

## Status: step 2 (2026-09-15)

`BallSprite.cs`, `PlayerSprite.cs`, `TeamData.cs` fully ported
(`swos_ball_sprite.{h,c}`, `swos_player_sprite.{h,c}`, `swos_team_data.{h,c}`)
including `PlayerSprite.Init()` and `TeamData.Init()`. `tests/test_sprites.c`
(new, 44 checks) covers: every offset re-derived independently from
`swos-port`'s packed `struct Sprite`/`struct TeamGeneralInfo` (auxiliary
verification only, per the corrected rule above — see each header's SOURCE
comment for the derivation), round-trips for every ported field including a
neighbouring-slot isolation check (catches a wrong `SlotStride`), and both
`Init()` functions' full contract (22-slot defaults, ordinal/team-number
assignment, pointer-table population, cross-team `opponentsTeam` pointers,
and confirming the documented "does NOT clear the energy padding region"
behavior). `make test` (both suites): 70/70 pass.

**Discrepancy summary vs OpenSWOS/swos-port** (informational only — nothing
below caused a code change, per the corrected fidelity rule):

- OpenSWOS's `OffAnimTablePtr`/`OffFrameIndicesTable` are 4-byte pointer
  fields; swos-port's `struct Sprite` has 2-byte fields at those same start
  offsets (`animTable`, `frameIndicesTable`) each followed by a 2-byte `tag`
  padding field. OpenSWOS deliberately widened each pair into one dword (its
  own `AnimationTablesData` system stores real Memory addresses, not
  swos-port's narrower in-table index) — confirmed by usage across
  `SpriteUpdate.cs`/`BallUpdate.cs`/`UpdatePlayers.cs`, all `ReadSignedDword`.
  Every other struct offset lines up exactly once this widening is accounted
  for (verified field-by-field through the full 110-byte struct). Ported as
  OpenSWOS defines it.
- `swos-port`'s `TeamGeneralInfo` has a `wonTheBallTimer` field (offset 138,
  between `AiBallSpinDirection`/136 and `goalkeeperPlaying`/140) that
  OpenSWOS's `TeamData.cs` never exposes as a named accessor. **Correction
  (after step 2's review): this is a gap in `TeamData.cs`'s semantic API
  only, not a gap in OpenSWOS's VM** — `PlayerActions.cs`, `PlayerControlled.cs`,
  `UpdatePlayers.cs` and `Kickoff.cs` all read/write the raw `+ 138` offset
  directly (confirmed by grep: e.g. `UpdatePlayers.cs:3620` `Memory.WriteWord(tackleOppBase + 138, 12)`,
  `Kickoff.cs:427` `Memory.WriteWord(teamBase + 138, 0)`), so the mechanic
  fully works in OpenSWOS today. When those files are ported (steps 5-8),
  each site's local `+ 138` literal gets ported as written — matching
  OpenSWOS's own inconsistency (accessor for most TeamData fields, raw
  offset for this one) rather than "fixing" it into a `swosTeamData*`
  accessor now, which would be adding something OpenSWOS itself doesn't
  have. A `swosTeamDataWonTheBallTimer()` accessor is a fine *later*,
  separate refactor once all four call sites are ported and can be updated
  together.
- OpenSWOS's `TeamData.OffOfs108` is an unidentified-field placeholder name;
  swos-port has since identified the same offset as `ballDirectionChangeTimer`.
  Cosmetic only (same offset, same 145-byte total struct size either way).
- Every other offset in all three files matches swos-port's packed struct
  layout exactly — no other disagreements found.

## Status: step 2.5 (2026-09-15) — `AnimationTablesData` + full `Memory.Init()`, golden-dump verified

`AnimationTablesData.cs` (`swos_anim_tables.{h,c}` + generated
`include/generated/swos_anim_streams.h`, 174 frame-indices arrays extracted
mechanically via `tools/extract_all_arrays.py`) and the full
`Memory.Init(bool pcMode)` (`src/swos_memory_init.c`) are ported, calling
`swosPlayerSpriteInit()` / `swosAnimTablesInit()` / `swosTeamDataInit()` in
the exact same order as the source. `swosMemoryInit(bool)` replaces the
step-1 `swosMemoryInitStub()` placeholder wherever realistic memory state is
needed (the stub is kept for tests that only want the Read/Write layer in
isolation).

**Gold-standard verification, per review request:** `tools/csharp-golden-dump`
is a standalone .NET console harness that compiles OpenSWOS's **actual,
unmodified** `SwosVm` source files in place (`<Compile Include="../../../openswos/...">`,
never copied) and runs the *real* `Memory.Init(true)` / `Memory.Init(false)`,
dumping the full `0x60000`-byte buffer to `build/golden/golden_{pc,amiga}.bin`.
`tests/test_golden_dump.c` then diffs the C port's own
`swosMemoryInit(true)` / `swosMemoryInit(false)` output against those files
byte-for-byte. This is strictly stronger than the round-trip/offset tests
elsewhere in this repo, which only prove internal self-consistency (a
setter and getter sharing the same wrong offset would still pass) — this
proves the C port matches an independent execution of the actual source
being ported.

**Result: byte-exact match, both modes, first try.** `0x60000` bytes each
for `pcMode=true` and `pcMode=false`, zero mismatches. Combined with the
earlier automated cross-check (every `Addr.X`/`Memory.Addr.X` reference in
`Memory.cs` and `AnimationTablesData.cs`'s `Init()` bodies present and used
correctly in the C port — 260 + 15 unique names, zero missing, zero extra),
this is about as strong a correctness signal as this layer can get without
running the original DOS binary itself.

Setting up the .NET SDK took a short detour (none was installed on this
machine — see memory `feedback-host-gcc-devkitpro-mingw64` for the sibling
note about the C host compiler situation); installed via
`winget install Microsoft.DotNet.SDK.8` with the user's approval. Regenerate
the golden files with:
```
cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
```
(Not wired up as a `make golden` target — invoking `dotnet` through `make`
on this machine fails inside NuGet for reasons that didn't isolate to
PATH/TMP/TEMP/HOME individually; not worth more time chasing since the
direct command works fine and `test_golden_dump.c` prints this same
instruction if the golden files are missing.)

`make test` (three suites): **72/72** pass (26 + 44 + 2).

## Status: step 3 (2026-09-16) — `SpriteUpdate`, differential-tested against real C#

`SpriteUpdate.cs` fully ported (`swos_sprite_update.{h,c}`):
`CalculateDeltaXAndY` (the direction/trig core every moving sprite uses),
`UpdateSpriteDirectionAndDeltas`, `MoveAllPlayers`, `SetNextPlayerFrame`
(including its goal-cheer overlay and the direction-change re-bind with its
goalie-save/injured-state suppression), `UpdateSpriteAnimation`,
`MoveSprite`, and the private `StopSpriteIfReachedDestination` /
`UpdateAnimationTableAndDestinationReached`.

**Forward dependency, handled per the "don't stub missing deps" rule:**
`SetNextPlayerFrame` and `UpdateAnimationTableAndDestinationReached` call
`PlayerActions.SetPlayerAnimationTable` — but `PlayerActions.cs` itself is
step 5. Rather than approximate it, the one self-contained function
actually needed (only touches Memory/PlayerSprite, both already ported) is
pulled forward verbatim into `swos_player_actions.{h,c}` — see that
header's comment. When step 5 ports the rest of `PlayerActions.cs`, extend
this file rather than re-porting the function into a second copy.

**Differential test against real C#, per review request** (covering
positive/negative deltas, stopping, and directions):
`tools/csharp-golden-dump/SpriteUpdateGolden.cs` runs the actual
`SpriteUpdate.CalculateDeltaXAndY`/`MoveSprite`/`UpdateSpriteAnimation`
against 14 + 5 + 4 representative scenarios (all 8 movement octants, zero
movement, zero speed, large/small deltas needing table-halving, overshoot
vs. non-overshoot stops on both axes, a stationary no-op case, and 4
animation-opcode fixtures covering plain loop / hold-last / variable-pause
/ negative relative jump) and dumps inputs+outputs to
`build/golden/sprite_update_golden.txt`. `tests/test_sprite_update_golden.c`
parses that file and recomputes the same scenarios via the C port —
**23/23 match byte-for-byte, first run.** (`CalculateDeltaXAndY` has no
PC/Amiga branch to test: OpenSWOS hard-locks it to PC mode unconditionally,
per its own source comment — so unlike `Memory.Init()`, there's no second
variant here.) Since `SpriteUpdate.cs` calls into `PlayerActions`, the C#
harness needed a stand-in for compilation — `PlayerActionsStub.cs`, a
**verbatim** copy of the real `SetPlayerAnimationTable` (diffed
line-for-line against the source to confirm), not an approximation; delete
it once step 5 adds the real file to the harness.

`make test` (five suites): **95/95** pass (26 + 44 + 2 + 23).

## Porting order (full plan)

1. ~~Memory, types, CPU flags, tables, RNG~~ (2026-09-15, see Status above)
2. ~~Sprite views: `BallSprite`, `PlayerSprite`, `TeamData`~~ (2026-09-15, see Status above)
   - ~~2.5: `AnimationTablesData` + full `Memory.Init()`, golden-dump verified~~ (2026-09-15, see Status above)
3. ~~`SpriteUpdate`~~ (2026-09-16, see Status above)
4. `BallUpdate`
5. `PlayerActions`
6. `PlayerControlled`
7. `UpdatePlayers`
8. `InputControls`
9. `AiHelpers`, `AiBrain`
10. `SetPieces`, `GameTime`, `Referee`
11. `GameLoop` orchestration
12. Adapter from VM state to the DS renderer (mirrors `swos-ds`'s
    `game_state.c` `swosTick()` boundary)

After each module: desktop build + tests, diff against OpenSWOS behavior
where practical, periodic `.nds` build once there's something to render.
