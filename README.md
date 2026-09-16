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

**Differential test against real C#, covering every public function** (not
just the three initially covered — see the follow-up review note below):
`tools/csharp-golden-dump/SpriteUpdateGolden.cs` runs the actual
`CalculateDeltaXAndY` / `MoveSprite` / `UpdateSpriteAnimation` /
`UpdateSpriteDirectionAndDeltas` / `SetPlayerAnimationTable` /
`SetNextPlayerFrame` / `MoveAllPlayers` against representative scenarios and
dumps inputs+outputs to `build/golden/sprite_update_golden.txt` (text) and
`sprite_pool_after_moveall.bin` (binary). `tests/test_sprite_update_golden.c`
replays every scenario through the C port and compares:

| Function | Scenarios | Coverage |
|---|---|---|
| `CalculateDeltaXAndY` | 14 | all 8 movement octants, zero movement/speed, table-halving |
| `MoveSprite` | 5 | +/- deltas, overshoot vs. not, stationary |
| `UpdateSpriteAnimation` | 4 | opcode interpreter: loop / hold / variable-pause / negative jump |
| `UpdateSpriteDirectionAndDeltas` | 5 | movement + no-movement, full 0..255 direction *and* the 0..7 quantised one |
| `SetPlayerAnimationTable` | 4 | team1/team2 outfielder, goalkeeper, null-frame-pointer early-return path |
| `SetNextPlayerFrame` | 7 | normal tick, direction-change rebind, rebind suppression (goalie dive / injured), both goal-cheer paths (scorer / teammate), keeper-excluded |
| `MoveAllPlayers` | 1 (full region) | entire 22×128-byte sprite pool, byte-exact, after one tick — also exercises the two private helpers (`StopSpriteIfReachedDestination`, `UpdateAnimationTableAndDestinationReached`), which have no C# entry point of their own to call directly |

**40/40 match byte-for-byte, first run** (`CalculateDeltaXAndY` has no
PC/Amiga branch to test: OpenSWOS hard-locks it to PC mode unconditionally,
per its own source comment — so unlike `Memory.Init()`, there's no second
variant here). Since `SpriteUpdate.cs` calls into `PlayerActions`, the C#
harness needed a stand-in for compilation — `PlayerActionsStub.cs`, a
**verbatim** copy of the real `SetPlayerAnimationTable` (diffed
line-for-line against the source to confirm), not an approximation; delete
it once step 5 adds the real file to the harness.

**Portability hardening (same review):** the sin/cos PC-damping shifts in
`CalculateDeltaXAndY` operate on values that can be negative, and C11
leaves `>>` on a negative signed operand implementation-defined (6.5.7p5) —
GCC has always treated it as arithmetic in practice, but the port must
match C#'s `int >> int`, which the language spec *guarantees* is
arithmetic. Rather than rely on "probably fine on this GCC" (and this repo
still has no ARM-toolchain test to spot-check it against), added
`swosAsr32()` (`include/swos_util.h`) — a portable arithmetic-shift
implementation using only well-defined unsigned operations — and switched
all 6 shift sites in `swos_sprite_update.c` to it. (Audited every other `>>`
site in `src/*.c`: `swos_memory.c`'s take unsigned operands, well-defined
regardless; `swos_rng.c`'s extract exactly one byte at 8/16/24-bit
boundaries, which is provably shift-mode-independent; `swos_sprite_update.c`'s
`dir8` computation operates on an always-non-negative value after `& 0xff`.
No other fixes needed.) Behavior unchanged — same 40/40 before and after.

**Review-requested gap closed:** the initial step-3 differential test only
covered 3 of `SpriteUpdate.cs`'s 5 public functions (leaving
`UpdateSpriteDirectionAndDeltas`, `SetNextPlayerFrame` — arguably the
riskiest function in the file, given its rebind/suppression/goal-cheer
logic — and `MoveAllPlayers` unverified beyond round-trip tests).
Follow-up review correctly called "pełne pokrycie różnicowe" premature;
the table above is the completed coverage.

`make test` (five suites): **112/112** pass (26 + 44 + 2 + 40, up from
95/95 with 23 `SpriteUpdate` checks before the follow-up).

## Status: step 4 (2026-09-16) — `BallUpdate`, plus a real dependency-graph discovery

`BallUpdate.cs` (1795 lines — `Tick()`/`TickPhysicsOnly()`, both goto-heavy
`Section3_ApplyDeltasAndBounce`/`Section4_GoalDetectionAndShadow`, spin/
kick-boost `ApplyBallAfterTouch`, and the small position/timer helpers) is
fully ported to `swos_ball_update.{h,c}`, `goto`/labels preserved verbatim
(C supports them natively, same as C#) rather than restructured into
if/else.

**The plan doesn't survive contact with the actual file graph, and that's
fine.** `BallUpdate.cs` calls two functions this repo hadn't touched:
`PlayerUpdate.UpdateBallWithControllingGoalkeeper` (`PlayerUpdate.cs`, 1553
lines) and `BallOutOfPlay.CheckIfBallOutOfPlay` (`BallOutOfPlay.cs`, 766
lines) — neither file appears anywhere in the original 12-step plan, which
treated "BallUpdate" as one monolithic step. Per the standing rule (no
stubs that fake gameplay logic, no silent scope quietly ballooning either),
the actual approach taken:

- `UpdateBallWithControllingGoalkeeper` (35 lines) turned out fully
  self-contained (Memory/BallSprite/PlayerSprite only) — pulled forward
  verbatim into `swos_player_update.{h,c}`, same pattern as step 3's
  `SetPlayerAnimationTable`.
- `BallOutOfPlay.cs` (whole file) ported in full to
  `swos_ball_out_of_play.{h,c}` — its own dependencies turned out to be
  `Memory`/`BallSprite`/`PlayerSprite`/`TeamData`/`Rng` (all already done)
  plus two more previously-unseen files, handled the same way:
  - `UpdateGoals.cs` (175 lines, `BumpTeamGoals` + `GoalScored`) ported in
    full to `swos_update_goals.{h,c}` — **except** the one call at the very
    end of `GoalScored` to `Result.RegisterScorer` (a scorer-list UI/stats
    side effect that needs `GameTime.cs`, a much later step, plus C#
    arrays that live entirely outside the emulated `Memory` buffer). That
    one call is `PORT_PENDING`: a `NULL`-defaulting function-pointer hook
    (`swosRegisterScorerHook`) in place of the call, not a stub pretending
    to run it — see `swos_update_goals.h`'s header comment. Nothing in
    `GoalScored`/`CheckIfBallOutOfPlay` branches on what `RegisterScorer`
    would have returned, so this doesn't affect match-simulation state or
    control flow, only a results-screen name list.
  - `MatchAudio.*` (11 call sites across `BallUpdate.cs`/`BallOutOfPlay.cs`)
    — confirmed by reading every call site: bodies are `Instance?.DoX()`
    into a Godot audio-player singleton, zero `Memory`/game-state writes.
    Omitted, not stubbed — there is no simulation logic in them to fake.
    Documented here rather than claimed as "full program parity": a real
    DS build will need its *own* audio triggers wired in at these exact
    points eventually, just never as part of this VM-fidelity layer.

**Differential tests, full VM/Memory state per review request:**
`tools/csharp-golden-dump/BallUpdateGolden.cs` runs the real
`BallUpdate.Tick()`/`ApplyBallAfterTouch()` through 25 scenarios (friction
under possession/free-ball/air/clamp-to-zero, small/loud/extreme-height
bounces, all three keeper-holds-ball Z branches including the
`UpdateBallWithControllingGoalkeeper` call, X/Y barrier bounces, upper/
lower goals, penalty bar-deflect, corner/throw-in/goal-out dispatch, and
every `ApplyBallAfterTouch` branch: left/right spin, high/normal kick
boost, long-pass boost, spin-timer expiry) and dumps the **entire
0x60000-byte buffer** per scenario (not just a function's return value).
`tests/test_ball_update_golden.c` replays each setup through the C port and
byte-compares the full buffer.

**25/25 match byte-for-byte, first run.** Since `BallUpdate.cs`/
`BallOutOfPlay.cs` need `MatchAudio` and `PlayerUpdate.cs`/`UpdateGoals.cs`
to even compile, the C# harness needed compilation stand-ins too —
`MatchAudioStub.cs` (no-op, matches the real `Instance?.DoX()` behavior
exactly — the real class can't be referenced anyway, it's a Godot `Node`
subclass this headless harness has no engine reference for),
`PlayerUpdateStub.cs` (verbatim `UpdateBallWithControllingGoalkeeper`,
diffed against the source to confirm), and `UpdateGoalsStub.cs` (verbatim
`BumpTeamGoals`/`GoalScored` minus the same `RegisterScorer` call the C
port defers, diffed against the source to confirm everything else is
unchanged). All three are compilation/scope stand-ins, not approximated
game logic.

`make test` (six suites): **137/137** pass (26 + 44 + 2 + 40 + 25).

## Status: ARM/BlocksDS checkpoint (2026-09-16)

After ~2800 new lines across steps 3-4, every test so far had only run
through desktop mingw64 gcc — never the actual target toolchain/CPU. Before
step 5, `nds-checkpoint/` (a separate BlocksDS project, own `Makefile`
copied and trimmed from `../swos-ds/Makefile`) cross-compiles the exact
same `../src`/`../include` sources (no copies) with
`arm-none-eabi-gcc -O2 -mthumb -mcpu=arm946e-s+nofp` and links a small
libnds console app (`source/main.c`) that re-runs 13 already
host-verified checks — exact expected values taken from
`tests/test_*.c`/the golden vectors, not re-derived — to catch anything
that only shows up on the real target: struct layout/alignment surprises,
UB that happened to behave on x86_64 but not ARM, stack depth through the
goto-heavy `Section4` state machine, etc. This is **not** the swos-ds game
— no rendering, no input, just a build+run smoke test.

**Result: clean build, zero warnings, 13/13 checks pass at runtime** (run
in melonDS — `[60/60] ALL CHECKS PASSED` on screen). Covers `Memory.Init`
(both pcMode variants), RNG, `CalculateDeltaXAndY` (the goto+Flags+
`swosAsr32`-shift-heavy trig core), `MoveSprite`, `PlayerSprite`/`TeamData`
init, and `BallUpdate` friction/bounce/goal-scoring. Binary size: `.text`
55 KB, `.bss` ~389 KB (dominated by the 384 KB `SWOS_MEM_SIZE` buffer,
landing in `.bss` since it's zero-initialized) — `.nds` is 125 KB, total
RAM footprint ~445 KB against the DS's 4 MB, no size/alignment/stack
surprises found.

## Status: step 5 (2026-09-16) — `PlayerActions.cs`, the rest of it

The remaining ~2050 lines of `PlayerActions.cs` (`SetPlayerAnimationTable`
was already done in step 3) are fully ported into `swos_player_actions.{h,c}`
alongside it — 20 more functions, all `goto`/labels preserved verbatim:
`GetPlayerInfoForSprite`, `UpdatePlayerWithBall`, `UpdateControllingPlayer`,
`CalculateIfPlayerWinsBall` (the 50/50 ball-duel resolution), `PlayerKickingBall`,
`PlayerHittingStaticHeader`, `PlayerHittingJumpHeader`,
`PlayerTackledTheBallStrong`/`Weak`, `DoFlyingHeader`/`DoLobHeader`, `DoPass`,
`SetPlayerDowntimeAfterTackle`, `SetJumpHeaderHitAnimTable` (distinct from
`SetPlayerJumpHeaderHitAnimationTable`, both ported), `GetClosestNonControlledPlayerInDirection`,
`SetPlayerAnimationTableAndPictureIndex`, `GetBallDestCoordinatesTable`,
`PlayStopGoodPassSampleIfNeeded`/`StopGoodPassSample`/`EnqueuePlayingGoodPassSample`,
`UpdatePlayerSpeedAndFrameDelay`/`RecomputeSpriteDeltas`.

**Scoping decisions, each grep-verified against the whole file before being
made — new dependencies pulled in only where the executing logic actually
needs them, nothing pulled in "just in case":**

- **Telemetry (~180 lines: shot counters, curve tracker, on-target
  tracker, `ResetShotCounters`/`TickShotCurveTracker`/`TickOnTargetTracker`/
  `RecordShot`/`CurveErrorNow`)** — confirmed by reading every one of them:
  they only mutate this file's own private C# statics, used for the
  `--swos-smoke` dev diagnostic, and never write `Memory`/`BallSprite`/
  `PlayerSprite`/`TeamData`. Omitted, documented, not stubbed — same
  treatment as `MatchAudio` in steps 3/4. The `FaithfulBallControl` flag
  (declared, never read anywhere in this file) is the same story.
- **`MatchAudio.*`** (`PlayKick`/`GoodTackleComment`/`HeaderComment`/
  `CancelGoodPass`/`EnqueueGoodPass`) — omitted at each call site. Two
  functions (`StopGoodPassSample`, `EnqueuePlayingGoodPassSample`) mix an
  audio call with a real `Memory` write; only the audio half is omitted,
  the `Memory`-affecting half is ported in full.
- **`PlayerControlled.IncSkillDuelOwnWin`/`IncSkillDuelOppWin`** — read the
  whole of `PlayerControlled.cs` (1881 lines, step 6, not otherwise touched)
  to confirm: one-line static counters, zero `Memory` effect. Omitted like
  the telemetry above.
- **`TeamDataLoader`** — forward-pulled as a **minimal slice**
  (`swos_team_data_loader.h`): just `PlayerInfoSize` and the seven skill
  offset constants (`OffPassing`..`OffFinishing`), needed because
  `GetPlayerInfoForSprite` resolves a sprite to a `PlayerInfo` record
  address and several functions then read skill bytes out of it.
  Deliberately **not** ported: `WritePlayerInfos`/`WireTeamFields`, the
  functions that actually *populate* those records from a loaded team file
  — a different layer (match setup / team-file loading) that pulls in
  `OpenSwos.Assets.TeamRecord`/`PlayerRecord`, `SkillScaling.cs`,
  `TeamPort.cs`, and a `Godot.GD.Print` call. `GetPlayerInfoForSprite`
  works correctly against a `PlayerInfo` block populated by any means (the
  differential tests poke one directly into `Memory`), so nothing here
  depends on those ever running.
- **`PlayerEnergy`** — OpenSWOS's own optional, off-by-default fatigue
  extension (its own header says original SWOS has no in-match stamina
  mechanic). Still part of what `PlayerActions.cs` actually calls today, so
  it's ported like anything else reached from here, not treated as
  "not really SWOS" and skipped. Forward-pulled as a **minimal slice**
  (`swos_player_energy.{h,c}`): only `EffectEnabled`/`ShotPenalty`/
  `SpeedStep`, the three members this file actually calls (grep-verified).
  The rest of `PlayerEnergy.cs` (`SeedSlot`/`DrainSlot`/`DrainOnTackle`/
  `RecoverAtHalfTime`/etc.) is called from other not-yet-ported files —
  port those calls when their callers are ported.

**Differential tests, full VM/Memory state, same pattern as steps 2.5/4:**
`tools/csharp-golden-dump/PlayerActionsGolden.cs` runs the real
`PlayerActions.*` through 31 scenarios (speed table + injury handicap +
fatigue penalty + pass-overlap boost + stoppage slowdown, both PlayerInfo-
wired and PlayerInfo-unwired skill-lookup paths, the ball-pinning helpers,
all three 50/50-duel outcomes, finishing/long/no-shot kicking, static/flying/
lob heading, strong/weak tackles CPU and human, pass-with-target/pass-with-
no-target, tackle-downtime CPU/human tables, the jump-header-hit animation
gate, and the good-pass-sample state machine) and dumps the entire
0x60000-byte buffer per scenario. `tests/test_player_actions_golden.c`
replays each setup through the C port and byte-compares the full buffer.

Getting the real `PlayerActions.cs` into the headless harness (replacing
step 3's `PlayerActionsStub.cs`, now deleted per its own header's
instruction) needed two new **minimal** compilation stand-ins —
`TeamDataLoaderStub.cs` (the same 8 constants as
`swos_team_data_loader.h`, verbatim) and `PlayerControlledStub.cs` (the
same two one-line counters, verbatim) — plus five more `MatchAudioStub.cs`
no-op methods for this file's audio call sites. `PlayerEnergy.cs` itself
has no Godot dependency (confirmed by grep) and is referenced directly, not
stubbed.

**31/31 match byte-for-byte, first run** (after fixing two scenario-setup
bugs in the C# harness caught by the C build/run before any comparison —
slot-vs-address argument mixups in `PlayerSprite.SetDirection` calls, and
one accidental "not a shot" branch from a wrong Y-coordinate gate — both
fixed in `PlayerActionsGolden.cs` before regenerating the golden dumps).

`make test` (six suites): **168/168** pass (26 + 44 + 2 + 40 + 25 + 31).

Per the user's explicit instruction, the ARM/BlocksDS checkpoint is **not**
repeated after this step alone — next one is after step 5.5, once both
player modules are joined.

## Status: step 5.5 (2026-09-16) — `PlayerUpdate.cs`, the rest of it

The remaining ~1490 lines of `PlayerUpdate.cs` (`UpdateBallWithControllingGoalkeeper`
was already done in step 4) are fully ported into `swos_player_update.{h,c}`
— goalkeeper AI: `GoalkeeperClaimedTheBall`, `TickGoalieDivingClaimCompletion`,
`GoalkeeperCaughtTheBall`, `TickGoalieCatchingBall`, `TickGoalieClaimed`,
`TickGoalkeeperHoldAutoRelease`, `GetFramesNeededToCoverDistance`,
`ShouldGoalkeeperDive`, `GoalkeeperJumping`, `GoalkeeperDeflectedBall`,
`RunShotTripWire`, `RunShotAtGoal` (+ its private `ApplyGoalScoredBranch`/
`ApplyGoalkeeperSavedBranch`/`OwnPlayersBase`/`OpponentPlayersBase` helpers)
— `goto`/labels preserved verbatim throughout, including the
`SwosShotChainExit` enum mirroring the asm's named jump targets.

**Scoping decisions, same discipline as step 5:**

- **Keeper-dive telemetry** (`s_diveCallsHigh`/`s_diveCallsLow`/
  `s_shouldDiveTrueCount` and their bump/reset/getter surface) — the C#
  source's own comment confirms this is a dev diagnostic ("useful for
  tracking regression on the dive bug fix"), zero `Memory` effect. Omitted
  at its three call sites, documented, not stubbed — same pattern as
  `PlayerActions.cs`'s shot counters.
- **`MatchAudio.KeeperClaimedComment`/`PlayKick`** — omitted (audio).
- **A `Godot.GD.Print("[PORT-SAFETY] ...")` debug line** in
  `TickGoalkeeperHoldAutoRelease` — a log statement, zero `Memory` effect,
  omitted.
- **`TeamPort.StopAllPlayers`** — forward-pulled as a **minimal slice**
  (`swos_team_port.{h,c}`): just `StopAllPlayers`/`StopPlayers`, the only
  `TeamPort` member this file calls. `TeamPort.cs`'s shot-chance-table
  machinery (`kGoalieSkillTables`, `UpdatePlayerShotChanceTable`, etc.) is
  a different layer (team-file-loading-adjacent setup), not called here.
- **`PortPlayerState` enum** — pulled forward *whole* (`swos_player_state.h`,
  16 tiny named byte values, no logic) from `UpdatePlayers.cs` (step 7),
  since it'll be needed unchanged by every future step that inspects
  `PlayerSprite.OffPlayerState` — cheaper to port once now than re-derive
  piecemeal later.
- **`TeamDataLoader`/`PlayerEnergy`** — both existing minimal-slice headers
  from step 5 extended with exactly what this step's functions call:
  `TDL_OFF_GOALIE_SKILL` (`RunShotAtGoal` reads `PlayerInfo.goalieSkill`)
  and `DrainOnKeeperCatch`/`KeeperSkillPenalty` (`GoalkeeperClaimedTheBall`/
  `RunShotAtGoal`'s fatigue hooks).

**Differential tests, full VM/Memory state, same pattern as steps 2.5/4/5:**
`tools/csharp-golden-dump/PlayerUpdateGolden.cs` runs the real
`PlayerUpdate.*` through 45 scenarios (every claim/catch/dive-completion
state transition, the CPU-only keeper-hold auto-release safety net's every
gate, `ShouldGoalkeeperDive`'s behind/front/penalty branches,
`GoalkeeperJumping`'s near/far/slower/random speed selection for both
teams, weak/strong deflection, the shot trip-wire's early exits, and
`RunShotAtGoal`'s forced-saved/goal-scored/saved-with-and-without-a-
committed-dive paths) and dumps the entire 0x60000-byte buffer per
scenario. `tests/test_player_update_golden.c` replays each setup through
the C port and byte-compares the full buffer.

Getting the real `PlayerUpdate.cs` into the headless harness (replacing
step 4's `PlayerUpdateStub.cs`, now deleted per its own header's
instruction) needed two new stand-ins: `GodotStub.cs` (a no-op `Godot.GD.Print`
— the one real Godot reference in this file, a debug log with no simulation
effect either side omits) and `PortPlayerStateStub.cs` (the same 16-value
enum as `swos_player_state.h`, verbatim, so `PlayerUpdate.cs` can compile
without pulling in the rest of `UpdatePlayers.cs`). `TeamPort.cs` itself has
no Godot dependency and is referenced directly, not stubbed.

**44/45 matched on the first run; the 45th (`hold_release_cpu_fires`)
caught a real bug — in the C# test harness, not the C port.**
`PlayerActionsGolden.cs`'s `kick_finishing_with_wired_skill_and_fatigue`
scenario (step 5) sets `PlayerEnergy.EffectEnabled = true` to exercise the
fatigue path but never reset it — a harmless-looking oversight *within*
that file (none of its own later scenarios happened to touch the fatigue
code path again), but `Program.cs` runs every `*Golden.Run()` in one
process, so the flag stayed stuck `true` into every one of
`PlayerUpdateGolden.cs`'s scenarios too. One of them called
`PlayerKickingBall` on a keeper with zero energy, which only *then* read as
"exhausted" and took a different finishing-skill bucket than intended —
exposing a real behavioral divergence in the test *setup*, not in either
port. Root-caused by probing `swosPlayerKickingBall` in isolation with the
exact same inputs (a throwaway `tests/zz_probe.c`, deleted after use) and
comparing against the two candidate skill tables directly. Fixed by
resetting `PlayerEnergy.EffectEnabled = false` right after that one step-5
scenario in `PlayerActionsGolden.cs`; regenerated golden dumps, reran both
`test_player_actions_golden.c` and `test_player_update_golden.c` — still
31/31 and now 45/45. Worth remembering: a `public static` field written by
a test scenario is state that outlives that scenario for the rest of the
process — reset it explicitly, don't rely on the next `Memory.Init()` (which
only resets `Memory`, not C#-side statics like `PlayerEnergy.EffectEnabled`).

`make test` (seven suites): **213/213** pass (26 + 44 + 2 + 40 + 25 + 31 + 45).

Both player modules (`PlayerActions.cs`, `PlayerUpdate.cs`) are now fully
joined.

## Status: ARM/BlocksDS checkpoint after step 5.5 (2026-09-16)

The checkpoint was rebuilt from scratch with the same port sources under
`src/` and `include/` (no copies), using
`arm-none-eabi-gcc -O2 -mthumb -mcpu=arm946e-s+nofp`. All 17 current C source
files compiled and linked with **zero warnings**.

The runtime slice was extended from 13 to **21 checks**. In addition to the
step-4 coverage, it now executes the newly joined player code on ARM:

- `PlayerActions.UpdatePlayerSpeedAndFrameDelay` plus delta recomputation;
- `PlayerUpdate.GoalkeeperClaimedTheBall`, including the hold-ball state and
  controlled-player wiring;
- `GetFramesNeededToCoverDistance`, including its original repeated-subtraction
  Q16.16 division;
- two named exits through the goto-heavy `RunShotAtGoal` state machine.

**Result in melonDS: 21/21, failed 0, `ALL CHECKS PASSED`.** Desktop remains
**213/213** against the real C# golden state. The step-5.5 checkpoint image is
131,584 bytes; the ARM9 ELF reports `.text` 62,200 bytes, `.data` 288 bytes,
and `.bss` 397,876 bytes (total runtime image about 460 KB, still with the
384 KB VM memory buffer dominating `.bss`). No ARM-only alignment, arithmetic,
optimization, or stack problem was observed.

## Status: step 6A (2026-09-16) — `PlayerControlled.cs`, human-team paths

The complete 1881-line `PlayerControlled.cs` control-flow body is now present
as `swos_player_controlled.{h,c}`. Because the source is deliberately written
like register-style C# (integer operations, labels, gotos, and Memory calls),
`tools/convert_player_controlled.py` performs the mechanical API/type rewrite
and generates the 1772-line C implementation. The generator is retained;
generated control flow must not be hand-edited.

All three public entries are ported: `RunControlledBranch`,
`RunPassReceiptTrigger`, and `RunPassExpectingBranch`, including the private
chase/friction helpers and 64-value `kBallFriction` table. The deferred duel
telemetry calls in `PlayerActions` are now connected, and
`FaithfulBallControl` has its step-6 owner.

Real dependency slices were pulled forward for
`PlayerHeader.PlayerAttemptingJumpHeader`/`AttemptStaticHeader` and
`PlayerTackle.PlayerBeginTackling`; these are actual OpenSWOS bodies, not
gameplay stubs.

**Intentional 6A/6B boundary:** CPU-only calls to
`AiBrain.SetControlsDirection` and `AiHelpers.AI_Kick` lead into step 9. C
exposes explicit hooks; entering a CPU path before step 9 asserts instead of
silently pretending AI ran. Step 6B installs the real AI implementations and
adds CPU-team differential fixtures together with step 9.

The golden harness now compiles the real, unmodified `PlayerControlled.cs`.
Twelve human-team scenarios compare the complete 0x60000-byte Memory buffer:
early penalty/break exits, stopped-direction handling, carrier ball pinning,
pass-receipt guards and commits, goalkeeper back-pass, out-of-pitch cancel,
plain chase, long-spin prediction, and receiver commit. **12/12 match C#
byte-for-byte; the full desktop suite is 225/225.** The full source set also
cross-compiles cleanly for ARM9 at `-O2` with zero warnings (build check; the
21-check runtime checkpoint from step 5.5 remains the latest target run).

## Status: step 7A (2026-09-16) — real local dependencies of `UpdatePlayers.cs`

Before porting `UpdatePlayers.cs` itself (4349 lines, step 7B), this step
ports everything it genuinely pulls in that isn't a distant future
porting-order step: `BallVariables.cs` (full file, 535 lines --
`UpdateBallVariables`/`CalculateBallNextGroundXYPositions`, the goto-heavy
ball-landing-position predictor `UpdatePlayers.cs` calls once per tick),
`TeamPort.UpdatePlayerShotChanceTable` (+ its two literal skill tables,
mechanically extracted -- see `tools/extract_team_port_tables.py`,
`kGoalieSkillTables`/`kPlayerShotChanceTable`, deferred in step 5.5),
`PlayerEnergy.DrainSlot`/`DrainOnTackle`/`InjuryRiskDoubled` (extending the
step-5/5.5 minimal slice), and the **entire remainder** of both
`PlayerHeader.cs` and `PlayerTackle.cs` -- `SetStaticHeaderDirection`/
`SetPlayerWithNoBallDestination` complete `PlayerHeader.cs`;
`PlayerTacklingTestFoul`/`TestFoulForPenaltyAndFreeKick`/
`TryBookingThePlayer`/`TrySendingOffThePlayer`/`PlayerTackled`/
`PlayersTackledTheBallStrong` complete `PlayerTackle.cs` (only its audio-stub
wrappers and `SwosRand` remain unported -- both already covered by the
established omission pattern and `swosRngNextByte()`).

**Corrected scope before porting anything:** an initial dependency scan
(comment-inclusive grep) suggested `UpdatePlayers.cs` also needed
`SetPieces.TickThrowIn` (~467 lines), `Referee.UpdateReferee`, and
`InputControls.UpdateControlledPlayer` -- re-checking with a comment-
filtered grep showed `InputControls`/`UpdateReferee`/`Kickoff.
PrepareForInitialKick`/`UpdateGoals.UpdatePostGoalRestart` appear **only in
comments**, never as real calls. The real, previously-missed set was
narrower: `BallVariables` (2 calls), the `TeamPort`/`PlayerEnergy`
extensions above, `PlayerHeader`'s remaining two functions, `PlayerTackle`'s
whole foul/booking/injury chain, and `Referee.ActivateReferee` (one real
call, from `PlayerTacklingTestFoul` when a foul draws a card).

**`Referee.ActivateReferee` -- minimal slice, not the whole 728-line
`Referee.cs`:** pulled forward with its private helpers
(`InitRefereeAnimationTable`, `MarkDisplaySpritesDirty`) and the
`RefereeSprite` Memory-view (a new Sprite outside the 22-player pool, at
`0x4FD00` -- clear of both TeamData-bottom, which ends `0x4FCFF`, and the
`0x4FE60+` scratch PlayerInfo region steps 5/5.5's tests already use). The
rest of `Referee.cs` -- the full per-tick referee-movement/card-animation
state machine (`UpdateReferee` and friends) -- is a different layer (per-
tick rendering/movement, not "a foul just happened, register it"), not
called from anything ported so far; it lands with its own future step.
`Referee.NotifyEnteredAboutToGiveCard` (called from `UpdatePlayers.cs`
directly) and `ActivateReferee`'s own `Dbg*` counters are pure telemetry
(zero `Memory` effect, confirmed by reading each one) -- omitted and
documented, not stubbed, same pattern as every other `*Golden` telemetry
omission in this port.

**Differential tests, full VM/Memory state:**
`tools/csharp-golden-dump/Step7AGolden.cs` runs the real dependency chain
through 24 scenarios (all four `UpdateBallVariables` direction branches
plus the not-moving case, both `CalculateBallNextGroundXYPositions` paths,
all three `UpdatePlayerShotChanceTable` paths, both `DrainSlot` states,
`SetStaticHeaderDirection`'s turn case, `SetPlayerWithNoBallDestination`
for outfielders on both teams and a goalkeeper, `ActivateReferee` directly,
`PlayerTacklingTestFoul`'s too-far/close-goalkeeper/yellow-card/no-cards/
injury-triggering paths, and both `PlayersTackledTheBallStrong` CPU/human
branches) and dumps the entire `0x60000`-byte buffer per scenario.
`tests/test_step7a_golden.c` replays each setup through the C port and
byte-compares the full buffer.

Getting the real `PlayerHeader.cs`/`PlayerTackle.cs` into the headless
harness (replacing the `PlayerHeader`/`PlayerTackle` stand-ins inside step
6A's `PlayerControlledDepsStub.cs`, now split and slimmed to
`AiStub.cs`) needed two new stand-ins of its own: `CameraStub.cs` (just
`GetCameraYWhole`, the one `Camera.cs` member `Referee.ActivateReferee`
uses -- not the rest of `Camera.cs`'s 573-line movement code) and
`RefereeStub.cs` (`ActivateReferee` + its private helpers + `RefereeSprite`,
verbatim, mirroring exactly what `swos_referee.c` ports). Five more
`MatchAudioStub.cs` no-op methods cover `PlayerTackle.cs`'s audio call
sites.

**24/24 match byte-for-byte, first run.** `make test` (nine suites):
**249/249** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24).

## Status: step 7B (2026-09-16) — `UpdatePlayers.cs` itself

The main per-team-per-tick orchestrator (4349 lines, ~24 functions, ~40
literal position tables) -- `swosUpdatePlayersUpdate(teamIndex)`
(`include/swos_update_players.h`, `src/swos_update_players.c`) mirrors
`Update(teamIndex)`'s goto-heavy control flow exactly: per-team timer
decay, `applyBallAfterTouch` + ball-location flags, the 11-player loop
(direction/`isMoving` copy, energy drain, `TACKLED`/`ROLLING_INJURED`
early-outs, ball-distance/height buckets, `DispatchByPlayerState`), and the
post-loop keeper-transition epilogue. Every other function stayed
`private static` in the C#, so it ports to a `static` file-local C function
in the same translation unit -- `checkIfThisPlayerGettingBooked`,
`tickGoalkeeper`, `runGoalkeeperInAreaChain` (the ~370-line
`l_ball_in_penalty_area`..`l_clamp_ball_y_inside_pitch` goto chain, largest
single function in the port so far), `runGoalieCantCatchBallPickup`,
`tickGoalieDiving` (including the dive-outcome verdict/claim/parry/weak-touch
chain), `goalkeeperRise`, `tickHumanControlled`, `overrideDestToBallIfChaser`,
`tickAiControlled` (CPU-team gate + stoppage tail), `setPlayerPositionsForGameBreak`,
`setPlayerWithNoBallDestinationForBreak`, `tickPassExpectingStopped`,
`tickTackledPlayer`, `tickTacklingPlayer`, `tickInjuredRollingPlayer`,
`updatePlayerBallDistanceAndHeight`, `tickEpilogueKeeperTransition`,
`ownGoaliePlayersBase`, `headerExitToNormal`, `tickJumpHeader`,
`tickStaticHeader`, `dispatchByPlayerState`.

**Literal tables, mechanically extracted:** all ~40 flat `short[]` position
tables (`kFreeKickFactorsX`, `kBottomStartingPositions`/`kTopStartingPositions`,
the `kDseg17Exxxx` foul-relative/corner/throw-in/penalty tables,
`kPlayersLeavingPitchTop`/`Bottom`, the second-half-restart and penalty-ring
tables) via `tools/extract_all_arrays.py` (the same batch flat-array
extractor from step 2.5) into `include/generated/swos_update_players_tables.h`
-- no hand-retyped numbers. The two nullable *jagged* dispatch tables
(`kTopBallOutOfPlayPositions`/`kBottomBallOutOfPlayPositions`, C#
`short[]?[]`) have no extractor support (`extract_table.py`'s brace-balanced
parser only handles flat arrays), so their *index-to-table-name* mapping is
hand-written in `swos_update_players.c` as `static const int16_t *const
[32]` arrays of pointers into the mechanically-extracted flat tables (plus
`NULL` at the C#'s `null` slots) -- the pointer *structure* is hand-placed,
but every numeric value it points at is still 100% mechanically extracted.

**Two deferred, assert-backed hook boundaries (not silent no-ops):**
- `AiBrain.SetControlsDirection`/`AiHelpers.AI_Kick` (step 9) reuse the
  *existing* `g_swosAiSetControlsDirectionHook`/`g_swosAiKickHook` globals
  from `swos_player_controlled.h` (established step 6A) -- no new hook
  infrastructure, just two more call sites wired to the same globals.
- `SetPieces.SetThrowInPlayerDestinationCoordinates`/`SetPieces.TickThrowIn`
  (step 10) are a **new** hook pair, `include/swos_set_pieces.h` +
  `src/swos_set_pieces.c`, mirroring the 6A assert-then-call convention
  exactly (`assert(hook != NULL && "... requires step 10"); if (hook)
  hook(...);`). Both are real, comment-filtered-grep-verified executed
  calls (`SetThrowInPlayerDestinationCoordinates` from
  `tickPassExpectingStopped`'s throw-in tail, `TickThrowIn` from the
  `PLSTATE_THROW_IN` dispatch arm) -- not comment-only references.

**Design decision, documented not silently dropped:** the C# wraps three
calls to `PlayerHeader.SetPlayerWithNoBallDestination` in `try { } catch
(System.Exception) { }` (C has no exceptions). The port calls
`swosSetPlayerWithNoBallDestination` directly, unguarded -- realistic
bounded `tacticsIdx`/ordinal values keep every reachable access in-bounds
(tactics are always seeded before `Update()` runs), and the C#'s own catch
body is a documented no-op (or, in one case, an explicit "must NOT return,
the pushback below still has to run" fall-through, which the unconditional
direct call already reproduces without needing the try/catch at all). See
`include/swos_update_players.h`'s header comment.

**Comment-filtered dependency audit (whole file):** re-ran the same
methodology as step 7A across all 4349 lines. Confirmed real: `TeamData`/
`PlayerSprite`/`BallSprite` accessors, `PlayerActions.*`, `PlayerEnergy.DrainSlot`,
`BallVariables.*`, `TeamPort.UpdatePlayerShotChanceTable`, `PlayerUpdate.*`
(goalkeeper chain), `PlayerControlled.RunControlledBranch`/
`RunPassReceiptTrigger`/`RunPassExpectingBranch`, `PlayerHeader.*`,
`PlayerTackle.*`, `AiBrain.SetControlsDirection`, `AiHelpers.AI_Kick`, and
the two `SetPieces` calls above. Confirmed comment-only (per the user's
explicit rule, not pulled in): `InputControls.UpdateControlledPlayer`,
`Referee.UpdateReferee`, `Kickoff.PrepareForInitialKick`,
`UpdateGoals.UpdatePostGoalRestart`. Audio (`MatchAudio.KeeperSavedComment`/
`PlayMissGoal`, both in `tickGoalieDiving`'s dive-outcome tail) and one more
telemetry call (`Referee.NotifyEnteredAboutToGiveCard`, `DbgEnteredAboutToGive++`
only) omitted after reading their bodies -- zero `Memory`/control-flow
effect, same standard as every prior step.

**`s_zeroTicksTop`/`Bot`, `s_carrierStallTicksTop`/`Bot` kept as real C
statics** -- port-only but gameplay-affecting debounce counters that gate a
real `Memory` write (the `controlledPlayer` re-election heuristic and the
teammate-hysteresis stall safety net), not telemetry despite living next to
telemetry-looking counters. `swosUpdatePlayersResetState()` resets them
between differential-test scenarios. `kChaseFallbackEnabled` (C# `const
bool`, currently `false`, gating a whole disabled-but-retained A/B heuristic
block) ports to `#define K_CHASE_FALLBACK_ENABLED 0` guarding a **runtime**
`if`, not `#if` -- the dead branch stays compiled and warning-checked so
flipping the `#define` to re-run the A/B can't silently bit-rot. Every
other C#-side telemetry counter in this file (`s_fallbackChasesTop`/`Bot`,
`s_kickFallbackTop`/`Bot` and its zone splits, `s_reaimAppliedTop`/`Bot`,
and their public getters -- `Main.cs` smoke-test reporting only) is omitted,
verified zero `Memory` effect.

**Differential tests, full VM/Memory state, through the ONE public entry
point:** every other function in `UpdatePlayers.cs` stayed `private static`
in the C#, so (unlike step 7A) per-function golden dumps aren't possible --
`tools/csharp-golden-dump/Step7BGolden.cs` instead seeds full match state
per scenario so `Update(teamIndex)`'s 11-player loop routes one targeted
sprite through the specific handler under test, while the other 10 take the
safe default. 11 scenarios: full in-progress orchestration (both teams),
a kickoff-stoppage tick exercising `setPlayerPositionsForGameBreak` +
both `kTop`/`kBottomBallOutOfPlayPositions` tables (both teams), and one
each for `tickTackledPlayer`, `tickTacklingPlayer`, `tickInjuredRollingPlayer`,
`tickJumpHeader`, `tickStaticHeader`, `tickGoalieDiving` (rise path), and
`checkIfThisPlayerGettingBooked`'s walk-to-referee branch. Every scenario
sets both teams' `playerNumber` to a human value -- deliberate, not a
coverage gap: every step-9/step-10 hook call site is gated on `playerNumber
== 0` (CPU team) or a throw-in state neither reaches, so this is what keeps
every scenario clear of both deferred-hook boundaries, exactly like 6A's
human-only scope before it. `tests/test_step7b_golden.c` replays each setup
through the C port and byte-compares the full buffer.

**11/11 match byte-for-byte, first run.** `make test` (ten suites):
**260/260** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11). ARM9/
BlocksDS cross-compile (`nds-checkpoint/`, same `-Wall` flags): clean, zero
warnings, `.nds` built successfully.

## Status: step 8 (2026-09-16) — `InputControls.cs`

The per-tick input + team-controls layer (`external/swos-port/src/controls/
gameControls.cpp`, 332 LOC; 1037 lines in the C#) --
`include/swos_input_controls.h`/`src/swos_input_controls.c`. Unlike
`UpdatePlayers.cs` (step 7B), every function here except four file-static
helpers is public in the C#, so most scenarios call the specific function
under test directly rather than routing everything through one entry point.
Ported in full: `resetGameControls`, `updateFireBlocked`,
`selectTeamForUpdate`, `updateTeamControls`/`postUpdateTeamControls`,
`getPlayerEvents`/`isPlayerFiring`/`isAnyPlayerFiring`,
`getFireStartedAndBumpFireCounter` (the quick-fire/normal-fire counter state
machine), `eventsToDirection`/`directionToEvents`, `filterOverlappedEvents`
(the up+down / left+right conflict resolver), and the two private
`updatePlayers.cpp`-adjacent functions `UpdateControlledPlayer`
(swos.asm:100851-101034 -- picks the closest eligible outfielder to the
ball as the team's controlled sprite during open play) and
`UpdatePlayerBeingPassedTo`/`...Stopped` (swos.asm:101045-101321 -- the
AI's "incoming pass" candidate selection, in-progress and stopped variants).

**Comment-filtered dependency scan (whole file) found exactly one new real
call:** `Bench.InBench()` (the bench-menu control-reset branch in
`UpdateTeamControls`) -- pulled forward as a minimal one-line slice
(`include/swos_bench.h`/`src/swos_bench.c`, `swosBenchInBench()` ==
`Memory.ReadSignedWord(g_inSubstitutesMenu) != 0`), not the rest of
`Bench.cs` (1868 lines, the substitutes-menu UI/state machine -- a
different layer, its own future step). No new gameplay-affecting
dependency required a hook or a stop-and-report.

**Deliberately not ported (documented, not stubbed):**
`DebugForceP1Direction`/`DebugForceP1Fire` (C#:104-105) are declared but
never *read* anywhere in the whole OpenSWOS tree (grep-verified across the
entire repo, not just this file) -- zero effect on `Memory`/control flow by
construction, not even telemetry. `StubZoomIn`/`StubZoomOut`
(`camera.cpp` `zoomIn`/`zoomOut`) have empty C# bodies (`/* TODO */`) --
porting an intentional no-op as a no-op is fidelity, not a gap.
`StubRequestFadeAndInstantReplay`/`...SaveReplay` are different and *are*
ported for real: both write a genuine `Memory` word (the replay-request
flag) even though the replay *consumer* isn't wired yet -- the C#'s own
comment explains the producer is wired ahead of the consumer on purpose.
Two `ic_*` diagnostic counter pairs (`CtrlSwapHuman{Top,Bot}`/
`CtrlSwapAi{Top,Bot}`) are real port-only telemetry (the C#'s own comment:
"the original keeps no such counters") -- kept as a small
`SwosInputControlsTelemetry` struct + `swosInputControlsResetTelemetry()`,
mirroring the established `SwosPlayerControlledTelemetry` pattern from step
6A.

**`eventsToDirection`/`directionToEvents` have no `Memory` side effects of
their own** -- a full-buffer diff can't observe a pure function's return
value directly. Exercised indirectly instead: `directionToEvents` via
`swosInputControlsSetJoystickState` (writes the resulting events bitmask to
`Memory`) and `eventsToDirection` via `swosUpdateTeamControls`'s internal
`updateTeamControlsInternal` (writes the resulting direction into
`TeamData.OffCurrentAllowedDirection`/`OffDirection`) -- both exercised with
varied direction/event combinations across the `UpdateTeamControls`
scenarios below.

**Differential tests, full VM/Memory state:**
`tools/csharp-golden-dump/Step8Golden.cs` runs 23 scenarios -- direct calls
for every public leaf function (`resetGameControls`; all three
`updateFireBlocked` branches; both `selectTeamForUpdate` parities;
`getPlayerEvents`/`filterOverlappedEvents`'s no-conflict/up-down/left-right
cases; `isPlayerFiring`/`isAnyPlayerFiring` true/false;
`getFireStartedAndBumpFireCounter`'s full press-hold-release state-machine
sequence; `setJoystickState` with and without a direction/fire;
`postUpdateTeamControls`'s header-or-tackle clear) plus seven
`updateTeamControls(top)` scenarios that route through it to exercise the
two private functions: human-team normal play (promotion + input + fire
latching), a CPU team (input branch skipped), a dead ball (no promotion),
a closest-candidate disqualified by `sentAway`+mid-tackle (third-closest
promoted instead), a game-stopped pass-to-player election, and a
bench-menu control reset. `tests/test_step8_golden.c` replays each setup
through the C port and byte-compares the full buffer.

**23/23 match byte-for-byte, first run.** `make test` (eleven suites):
**283/283** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23).
ARM9/BlocksDS cross-compile: clean, zero warnings, `.nds` built
successfully.

## Status: step 9 (2026-09-16) — `AiHelpers.cs` + `AiBrain.cs`, closing the 6A/6B boundary

`AiHelpers.cs` (the small AI helpers, ~700 LOC of the original asm) --
`include/swos_ai_helpers.h`/`src/swos_ai_helpers.c` -- ports `AI_Kick`,
`AI_SetDirectionTowardOpponentsGoal`, `AI_DecideWhetherToTriggerFire`,
`AI_ResumeGameDelay`, and `FindClosestPlayerToBallFacing` in full. Every
real call in this file is to `Memory`/`TeamData`/`PlayerSprite`/`BallSprite`,
all already ported -- zero new dependencies.

`AiBrain.SetControlsDirection` (`updatePlayers.cpp:15980`, ~3333 LOC of the
original asm, the largest single AI function in SWOS) --
`include/swos_ai_brain.h`/`src/swos_ai_brain.c` -- runs once per team per
tick and decides whether to press fire, what direction to face, and what
ball-spin/after-touch strength to apply. **Fidelity note:** the C# source's
own top-of-file comment describes an earlier, partially-stubbed porting
increment ("Port scope (this commit)" / "Stubs: ..."); that language turned
out to be stale documentation from an early commit -- verified by reading
the entire current 1772-line file end to end: every `goto` label resolves
to a real terminal `return` or a real continuation, with zero remaining
`// TODO`/stub markers in the function body itself (the two mentions of
"TODO"/"stub" in the file are both inside that historical header comment,
which the C# source never updated after finishing the port). Ported in
full, including the two Amiga-mode direction-flip helper functions
(`checkForAmigaModeDirectionFlipBan`/`writeAmigaModeDirectionFlip`,
`amigaMode.cpp:64-81`) and their shared `s_amigaPreventDirectionFlip`
file-static -- dead in this port's PC-only configuration (`GameTime.
AmigaModeActive()` is hardcoded `false`) but kept live, not hardcoded away,
matching the C#'s own documented intent ("swapping to Amiga mode is one
flag flip").

**Comment-filtered dependency scan found one new real dependency, a
trivial one-liner:** `GameTime.AmigaModeActive() => false` -- pulled
forward as a minimal slice (`include/swos_game_time.h`/`src/swos_game_time.c`,
`swosGameTimeAmigaModeActive()`), not the rest of the 1736-line
`GameTime.cs` (match-clock orchestration, its own future step). Everything
else (`AiHelpers.*`, `Rng.NextByte`, `SpriteUpdate.CalculateDeltaXAndY`,
`Memory`/`TeamData`/`PlayerSprite`/`BallSprite` accessors) was already
ported. No large new dependency chain turned up, so no stop-and-report was
needed.

**Closing the 6A/6B boundary:** `g_swosAiSetControlsDirectionHook`/
`g_swosAiKickHook` (`swos_player_controlled.h`, established step 6A) are
now statically initialized to the real `swosAiBrainSetControlsDirection`/
`swosAiHelpersAiKick` -- `SwosAiSetControlsDirectionHook
g_swosAiSetControlsDirectionHook = swosAiBrainSetControlsDirection;` at the
one definition site (`src/swos_player_controlled.c`, regenerated by
`tools/convert_player_controlled.py` -- the *generator's own template* was
updated, then re-run, rather than hand-patching the generated file, so a
future regeneration can't silently revert the wiring). The `requireAi*`
wrapper functions (in both `swos_player_controlled.c` and
`swos_update_players.c`, which reuses the SAME globals) are kept as named,
stable call points -- per the user's explicit instruction -- but their
asserts dropped the "requires step 9" framing for a plain defensive
null-check, since the hook can now never be NULL outside a deliberate test
override. No parallel AI-calling convention was introduced anywhere.

**Telemetry omitted** (`AiBrain`'s 8 per-branch entry counters +
`ResetFireSiteCounters`): pure C#-side ints with public getters ("so the
smoke test can verify the AI's real fire paths are reached"), never read
by any `Memory` write or control-flow branch, zero RNG consumption --
same pattern as every prior step's telemetry omissions.

**RNG-consumption discipline (explicitly checked, per this step's
instructions):** `AiBrain.SetControlsDirection` draws exactly one `Rng`
byte per call (`AI_rand`, right at the top), which drives many downstream
branch choices (`aiRand & 0xF`, `& 7`, `& 3`, `& 1`, `& 0x18`, ...). `Rng`
state is a *separate* set of statics from `Memory` on both sides of the
port -- confirmed by reading both `swos_rng.c` (file-local `s_seed` etc.,
untouched by `swosMemoryInit`) and `Memory.cs` (same separation) -- so it
is NOT reset by `Memory.Init()`/`swosMemoryInit()` beyond the one
deterministic `Rng.Reseed(ReadWord(Addr.currentGameTick))` call already at
the tail end of `Init()` itself (mirrored in `swos_memory_init.c`;
deterministic because `currentGameTick == 0` right after a fresh `Init()`).
Every new differential-test scenario that calls `AiBrain.SetControlsDirection`
still explicitly re-seeds with its own chosen seed immediately after
`Init()`/`swosMemoryInit()` -- rather than relying on "whatever Init's own
reseed happened to produce" -- using the SAME seed on both the C# and C
sides, picked by probing `Rng.Reseed(N); Rng.NextByte()` for the specific
low-bit pattern each branch gate needs (documented per-scenario in
`Step9Golden.cs`). Also checked: `AiBrain`'s `s_amigaPreventDirectionFlip`
file-static is NOT reset by `Memory.Init()` either, but since
`AmigaModeActive()` is hardcoded `false`, `checkForAmigaModeDirectionFlipBan`
always leaves it `false` and the write branch in
`writeAmigaModeDirectionFlip` is unreachable in every scenario -- verified
by reading both helpers' bodies, not assumed, so no cross-scenario
contamination risk exists for it in this configuration.

**Differential tests, full VM/Memory state, all public AI entry points:**
`tools/csharp-golden-dump/Step9Golden.cs` runs 27 scenarios -- 16 covering
`AiBrain.SetControlsDirection`'s major branch families (the three top-level
early-out gates; the game-over halftime/full-time auto-fire branch; five
game-not-over stoppage set-piece dispatches by `gameState`
keeper-holds-ball/goal-scored/throw-in/free-kick/penalties; the
game-in-progress penalty/spin-timer fast path; no-controlled-player early
return; the player-near fire-decision both outcomes; a successful chase
committing to `l_our_player_closest`; the three no-one-near outcomes
-- pass-target reassignment, random 90-degree flip, and use-current-direction;
and three `l_ball_after_touch_allowed` spin/strength outcomes), 4 for
`AiHelpers.cs`'s standalone functions (`AI_Kick`, `AI_SetDirectionTowardOpponentsGoal`,
`AI_DecideWhetherToTriggerFire` true/false), and 3 CPU-team integration
scenarios proving the previously-asserting branches in
`PlayerControlled.RunControlledBranch` and `UpdatePlayers.Update`
(off-ball `AI_SetControlsDirection`, and `TickPassExpectingStopped`'s
`AI_Kick` call site) now run the real AI end to end.
`tests/test_step9_golden.c` replays each setup through the C port and
byte-compares the full buffer.

**27/27 match byte-for-byte, first run.** `make test` (twelve suites):
**310/310** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23 + 27).
ARM9/BlocksDS cross-compile: clean, zero warnings, `.nds` built
successfully.

## Status: step 10 (2026-09-16) — `SetPieces.cs`, `GameTime.cs`, `Referee.cs`, plus `Result.cs`

Comment-filtered dependency scan (per this step's own instructions, before
writing any code) found all three files' dependencies already ported except:
`SetPieces.AdvancePenaltiesTimer` → `GameTime.NextPenalty` (this step);
`GameTime.NextPenalty` → `GameLoop.PlayersLeavingPitch` and
`InitPitchBallFactors` → `BallSim.CurrentPitchType`; `Referee.cs` (its
`ActivateReferee` already forward-pulled in step 7A) → `Camera.GetCameraXWhole`
(joining the existing `GetCameraYWhole`) and a new `BookedPlayerNumberSprite`
Memory view. Porting order followed the dependency chain: `GameLoop.
PlayersLeavingPitch` (minimal slice) → `GameTime.cs` (full) → `Referee.cs`
(rest of it) → `SetPieces.cs` (full, its only new dependency now satisfied).

**`GameLoop.PlayersLeavingPitch` — forward-pulled whole, not the rest of
`GameLoop.cs`:** `include/swos_game_loop.h`/`src/swos_game_loop.c`. Small
(29 lines), fully self-contained (`Memory`/`TeamData`/`TeamPort` only, all
already ported) — same pattern as `GameTime.AmigaModeActive()` (step 9) or
`UpdateBallWithControllingGoalkeeper` (step 4). The rest of `GameLoop.cs`
(~1900+ lines, the full per-tick orchestrator) is step 11.

**`BallSim.CurrentPitchType` — port-only C-side global, not a `Memory`
slot:** `BallSim` lives in `BallState.cs` (470 lines) — a completely
different, idiomatic ball-physics layer built on its own `Fixed`/`BallState`
type family, not part of the `SwosVm.Memory`-based mechanical-port family
at all. `InitPitchBallFactors` reads exactly one member (grep-verified), so
only that one value is ported (`g_swosBallSimCurrentPitchType`, default 4 =
Normal) as a plain C global in `swos_game_time.c` — same treatment as
`PlayerEnergy.EffectEnabled`/`TimeDeltaOverride`.

**`Result.cs` checked before extending scope, per this step's instructions
— does NOT open a large independent chain:** 407 lines — `RegisterScorer`
(scorer list) + the result-display timer state machine
(`UpdateResult`/`HideResult`/`ShouldDrawResult`) + `ResetResult`.
`RegisterScorer`'s one real dependency is `GameTime.GameTimeAsBcd()`,
ported this same step. The scorer list (`m_team{1,2}Scorers`) and team-name
cache live in plain C#-side statics outside the emulated `Memory` entirely
(the C#'s own comment: "we don't add 8×10 byte arrays to Memory.cs because
they're nested 3-deep") — ported as plain C statics
(`include/swos_result.h`/`src/swos_result.c`). Full file ported; wired
`swosRegisterScorerHook` (NULL-defaulting since step 4's
`swos_update_goals.h`) to the real `swosResultRegisterScorer`, closing that
`PORT_PENDING` boundary.

**Omitted (documented, not stubbed, zero `Memory` effect — confirmed by
reading each one):** audio (`MatchAudio.PlayEndGameWhistle`/
`EnqueueRedCard`/`EnqueueYellowCard` — pure playback, same pattern as every
prior step) and the entire `HalftimeCeremonyStage`/
`SetHalftimeCeremonyStage`/`s_halftimeCeremonyStage` chain — the C#'s own
comment claims it's "kept for Main.cs compile compatibility", but a
whole-tree grep shows ZERO references anywhere outside `GameTime.cs` itself
(not even in `Main.cs`) — genuinely dead code, same category as
`InputControls.cs`'s `DebugForceP1Direction`/`DebugForceP1Fire` (step 8).
Also the whole clock-digit rendering chain (`DrawGameTime`/
`DrawGameTimeImpl`/`GetGameTimeSprites`/`GetSpriteWidth`/
`StubDrawMenuSprite`) — `StubDrawMenuSprite` is ALREADY a no-op in the C#
source (`/* TODO */`, no menu sprite-descriptor stream loaded), so this
entire call chain has zero `Memory` effect end to end; nothing to port
beyond a no-op the source already documents as one.

**A real static kept, not telemetry:** `s_stoppageRealTicks`
(`GameTime.cs`) looks like a presentation-only "+M:SS" injury-time counter,
but it gates a real control-flow branch in `UpdateGameTime` (the
last-minute-prolong backstop) — ported as a genuine C static, not omitted.
Reset by `swosGameTimeResetGameTime()`, matching the C#.

**RNG and static-state discipline (explicitly checked):** `Rng` stays a
separate static set from `Memory`, as established. Every function drawing
`Rng` bytes this step (`TickPenalty`, `StartFirstExtraTime`/
`StartPenalties`, `MarkPlayersHappyOrSad`, `ActivateReferee`/
`PutRefereeToLeavingState`) gets an explicit matching reseed on both sides
wherever the branch outcome depends on it. Separately:
`GameTime.s_stoppageRealTicks` and `Result.cs`'s scorer-list statics are
C#-side statics that persist across the whole golden-dump process (every
`*Golden.Run()` shares one process) — every scenario touching either calls
`GameTime.ResetGameTime()`/`Result.ResetResult()` first so state from an
earlier scenario can't leak in.

**Differential tests, full VM/Memory state, every public entry point of all
four modules:** `tools/csharp-golden-dump/Step10Golden.cs`, 59 scenarios —
full coverage of `SetPieces.cs` (both `SetThrowInPlayerDestinationCoordinates`
branches, the whole `TickThrowIn` state machine — AI/human, quick/normal
fire, mid-countdown, abort-on-wrong-gameState — `DispatchByGameState`'s
three routes, `TickSetPieces` and all four resolvers, `TickFreeKick`,
`TickPenalty`'s both branches, `AdvancePenaltiesTimer`'s both branches),
`GameTime.cs` (the clock lifecycle — normal tick, minute rollover, prolong
pin/refresh, `EndFirstHalf` firing past a draining goal celebration,
accessors, `MarkPlayersHappyOrSad`'s both outcomes, `NextPenalty`'s both
outcomes, every `initMatch()`-adjacent helper), `Referee.cs` (the whole
`UpdateReferee` state machine — incoming walk, off-screen movement,
inactive, about-to-give-card→booking for yellow/red, leaving→off-screen,
`UpdateBookedPlayerNumberSprite`'s blink-on and sentinel-sends-off paths,
`RemoveReferee`, accessors), and `Result.cs` (the full `UpdateResult`
lifecycle, `RegisterScorer` for a regular goal/own goal/second goal by the
same scorer). Four integration scenarios per the review request: a full
throw-in cycle, corner-then-free-kick direction update, a card activating
the referee and walking them in, and time passing to half-time with the
result panel showing. `tests/test_step10_golden.c` replays each scenario
through the C port and byte-compares the full `0x60000` buffer.

**59/59 match byte-for-byte, first run.** `make test` (thirteen suites):
**369/369** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23 + 27 +
59). ARM9/BlocksDS cross-compile: clean, zero warnings, `.nds` built
successfully (build-only check, same as step 6A — no runtime-checkpoint
expansion this step).

Closes the last deferred hook from step 4 (`swosRegisterScorerHook`).

## Status: step 11A (2026-09-16) — real local dependencies of `GameLoop.cs`

Comment-filtered dependency scan of `GameLoop.cs` (2045 lines) found it
pulls in SEVEN previously-untouched files, ~3900 more lines total --
substantially bigger than the porting-order list's one-line "`GameLoop`
orchestration" suggested. Reported to the user before committing to the
work (per the project's stop-and-report convention), then split like the
step-7A/7B precedent: this step (11A) ports the six smaller/self-contained
dependencies plus a minimal `Bench.cs` extension; step 11B will be
`GameLoop.cs` itself; a dedicated full port of `Bench.cs` (see below) is
its own step in between.

| File | Lines | What `GameLoop.cs` calls | This step's scope |
|---|---|---|---|
| `Kickoff.cs` | 525 | `PrepareForInitialKick`, `ReseatTeamsForNewHalf` | minimal slice (~140 lines) |
| `Camera.cs` | 573 | `MoveCamera`, `SetCameraX/Y` | full file |
| `GameSprites.cs` | 314 | `UpdateCornerFlags`, `UpdateControlledPlayerNumbers` | full file |
| `SpinningLogo.cs` | 95 | `UpdateSpinningLogo` | full file |
| `PlayerNameDisplay.cs` | 231 | `UpdateCurrentPlayerName` | full file (minus pure text render) |
| `Stats.cs` | 286 | `UpdateStatistics` | full file |
| `Bench.cs` | 1868 | `UpdateBench`, `CheckIfGoalkeeperClaimedTheBall` | **not this step** — see below |

**`Kickoff.cs` — minimal slice, not the whole 525-line file:**
`include/swos_kickoff.h`/`src/swos_kickoff.c`. Only `PrepareForInitialKick()`
(the no-arg overload — the `int` overload is a "PORT-COMPAT shim -- remove
after Main.cs rewire" per its own comment, dead code) and
`ReseatTeamsForNewHalf()` (+ 3 tiny private helpers) are real calls.
Deliberately not ported: `StartingMatch`/`InitPlayersBeforeEnteringPitch`/
`DetermineStartingTeamAndTeamPlayingUp` (match-boot-only, not called from
anything ported yet; the latter duplicates
`swosGameTimeDetermineStartingTeamAndTeamPlayingUp` from step 10) and the
`KTeamsStartingCoordinates`/`BottomStartingPositions`/`TopStartingPositions`
tables (the latter two already mechanically extracted in step 7B).

**Camera.cs, GameSprites.cs, SpinningLogo.cs, PlayerNameDisplay.cs,
Stats.cs — ported in full** (`include`/`src` `swos_camera`, `swos_game_sprites`,
`swos_spinning_logo`, `swos_player_name_display`, `swos_stats`): each
file's single public entry point (`MoveCamera`, `UpdateCornerFlags` +
`UpdateControlledPlayerNumbers`, `UpdateSpinningLogo`,
`UpdateCurrentPlayerName`, `UpdateStatistics`) reaches almost the entire
private surface of its own file, so "minimal slice" wasn't available —
these are small enough (95-573 lines) that a full port was the right size
anyway. All five were already fully self-contained (`Memory`/`TeamData`/
`PlayerSprite`/`BallSprite`/`Rng`/`Referee.CardHandingInProgress`/
`Result.HideResult`, all already ported) except for one shared dependency:
`Bench.InBench()`/`InBenchMenus()`/`GetBenchState()` — extended into the
existing step-8 minimal `Bench.cs` slice (still just 3 tiny accessors, not
the rest of the file).

**Debug prints omitted, matching every prior step's telemetry pattern:**
`GameSprites.UpdateControlledPlayerNumbers`'s debounced out-of-range-shirt
diagnostic (`Godot.GD.PrintErr`) — the print is omitted, but the debounce
state (`s_lastBadShirtOrdinal`) and the guarded branch that hides the digit
instead of drawing a wrong number ARE ported, since that branch has a real
`Memory` effect.

**`Bench.cs` deliberately NOT ported this step:** checked whether
`UpdateBench`/`CheckIfGoalkeeperClaimedTheBall` could get the same
minimal-slice treatment as everything else here — they can't.
`UpdateBench()` runs unconditionally every tick; its "not currently in the
bench" branch alone (`BenchBlocked`/`BenchUnavailable`/
`GetNonBenchControlsTeam`/`UpdateNonBenchControls`/`BenchInvoked`) already
reaches most of the file's private surface, and the moment a human opens
the substitutes menu it needs the full menu/substitution FSM.
`UpdateBench()` also hosts `UpdateSubstitutedPlayerWalk()` — a real
per-tick gameplay FSM that `UpdatePlayers.cs` (step 7B) deliberately left
as a TODO, relocated here per its own comment. This is a real, whole-file
dependency (1868 lines) — its own dedicated step, between this one and
11B (`GameLoop.cs` itself).

**Differential tests, full VM/Memory state, every public entry point of
all six modules touched this step:**
`tools/csharp-golden-dump/Step11AGolden.cs`, 40 scenarios — `Kickoff`
(both starting-team branches of `PrepareForInitialKick`, the team-identity
swap in `ReseatTeamsForNewHalf`), `Camera` (accessors, the fans-counter
early-out, the (0,0) self-heal, all five `MoveCamera` mode branches --
booking/penalty-shootout/bench/leaving-bench/standard -- and three
`StandardMode` sub-branches -- follow-ball/waiting-for-players/result-
screens --, `SetCameraToInitialPosition`, `SwitchCameraToLeavingBenchMode`),
`GameSprites` (corner-flag animation, the controlled-player-number gate/
shown/marked-hidden paths, the face-offset helpers), `SpinningLogo`
(disabled, spinning, blocked-by-bench-menus), `PlayerNameDisplay` (scorer
blinking, card blinking, prolong-last-before-goalkeeper, in-progress
show/hide, stopped hide-first-frame/prolong), `Stats` (init, both toggle
branches, possession bump, goal-attempt registration, penalties skip,
auto-hide), and the `Bench.InBenchMenus` extension (both state branches).
`tests/test_step11a_golden.c` replays each scenario through the C port and
byte-compares the full `0x60000` buffer.

**40/40 match byte-for-byte, first run.** `make test` (fourteen suites):
**409/409** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23 + 27 +
59 + 40). ARM9/BlocksDS cross-compile: clean, zero warnings, `.nds` built
successfully (build-only check).

## Status: step 11 (2026-09-16) — full port of `Bench.cs`

The dedicated `Bench.cs` step flagged at the end of step 11A is done: the
whole 1868-line file, extending the existing 3-accessor minimal slice
(`InBench`/`InBenchMenus`/`GetBenchState`, steps 8/11A) into the full
substitution/bench-menu module (`include/swos_bench.h`/`src/swos_bench.c`).

**Almost the entire file is private surface reached through two public
entry points** (`UpdateBench`/`BenchCheckControls`, same shape as step 7B's
`UpdatePlayers.cs`), plus ~25 small public accessors. Ported in full:
the out-of-bench "is the bench being summoned" poll (`BenchBlocked`/
`BenchUnavailable`/`GetNonBenchControlsTeam`/`UpdateNonBenchControls`/
`BenchInvoked` — including the tap-counter direction-detector verified 1:1
against the SWOS disassembly per the C#'s own header note), the full menu
FSM (`BenchState` initial/about-to-substitute/formation/marking-players,
each with its own handler), substitution (`InitiateSubstitution`/
`SubstitutePlayer` — PlayerInfo record swap, sprite-content swap keeping
ordinals, shirt-number table swap, frame-index re-derivation), tactics
change, and the substituted-player walk-on/walk-off state machine
(`UpdateSubstitutedPlayerWalk` — the real per-tick FSM `UpdatePlayers.cs`
step 7B left as a TODO, hosted here per the C#'s own documented choice,
stepped first in `UpdateBench` to preserve the original tick order).

**A subtle, easy-to-miss divergence caught by reading the whole file, not
just its call sites:** `Bench.cs` has its OWN private `StopAllPlayers()`
(team.cpp:26-44) — a *different* function from the already-ported
`TeamPort.StopAllPlayers()` (step 5.5), despite sharing a name. The two
disagree on one bit: `TeamPort.StopAllPlayers()` always clears both teams'
`goalkeeperPlaying` (production behavior, no `SWOS_TEST` flag wired);
`Bench.cs`'s own copy deliberately preserves the *original SWOS bug* where
the top team's `goalkeeperPlaying` is never cleared (per its own comment).
Ported as two distinct C functions, matching the C# source's own
duplication — reusing `swosTeamPortStopAllPlayers()` here would have
silently "fixed" a bug the port is chartered to reproduce exactly.

**Debug-only surface omitted** (documented, not stubbed, zero `Memory`
effect): `DebugTapStateString`/`DebugLastPollGameStatePl`/
`DebugLastPollBlocked`/`DebugLastPollUnavailable` — pure diagnostic
captures for the original's `--bench-test` harness. The underlying calls
they capture (`BenchBlocked`/`BenchUnavailable`) are still made in full —
only the extra debug-field storage is omitted, confirmed by reading each
one (they tick down real `Memory` timers with real gameplay consequences).

**Extended, not duplicated:** `TeamDataLoader`'s minimal-slice header
(steps 5/5.5) gained `OffSubstituted`/`OffCards`/`OffFace` (the
eligibility checks and post-swap frame re-derivation need them,
comment-filtered-grep verified against the whole file).

**Differential tests, full VM/Memory state:**
`tools/csharp-golden-dump/Step11BenchGolden.cs`, 28 scenarios — every
public accessor, both `InitBenchBeforeMatch` variants, out-of-bench polling
(blocked by timer/referee, unavailable during play/ceremonies, CPU teams
never invoke, single tap doesn't invoke, triple tap invokes, secondary-fire
invokes), `InvokeBench` (normal, mid-throw-in cleanup, keeper-holds-ball
claim), in-bench menu navigation (arrow selection, coach-row → marking
menu, substitute-row → about-to-substitute with exact-position matching,
formation menu → `ChangeTactics`, leave via left/right motion), a full
`InitiateSubstitution` → walk-FSM → `SubstitutePlayer` cycle verifying the
PlayerInfo/sprite/shirt-number swap, the walk FSM's three direct states
(still travelling, stretchered shortcut, settled completion), and
`CheckIfGoalkeeperClaimedTheBall`'s both branches (including the preserved
top-team `goalkeeperPlaying` bug). `tests/test_step11_bench_golden.c`
replays each scenario through the C port and byte-compares the full
`0x60000` buffer.

**28/28 match byte-for-byte, first run.** `make test` (fifteen suites):
**437/437** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23 + 27 +
59 + 40 + 28). ARM9/BlocksDS cross-compile: clean, zero warnings, `.nds`
built successfully (build-only check).

Only `GameLoop.cs` itself (2045 lines, step 11B) remains before step 12.

## Status: step 11B (2026-09-16) — `GameLoop.cs` itself, completing step 11

The per-tick orchestrator itself is ported in full
(`include/swos_game_loop.h`/`src/swos_game_loop.c`, extending the step-10
`PlayersLeavingPitch` minimal slice into the whole file). Every real
dependency this file reaches had already landed in steps 10, 11A, and the
dedicated `Bench.cs` step, so this was a pure orchestration port with no
new forward dependencies.

**Ported in full:** `Tick`/`UpdateTimers`/`CoreGameUpdate` (the per-tick
pipeline calling every subsystem in the original's exact order),
`UpdateFireBlocked`/`SelectTeamForUpdate`, the ~1400-line
`UpdateGameTimersAndCameraBreakMode` state machine (penalty-shootout
inter-pen pause, the `ST_WAITING_ON_PLAYER` accumulator with its CPU
825-tick safety net and the port-only last-resort force-kick fallback, the
fire-press ceremony-skip paths for gameState 21/22/25/26, the
`stoppageEventTimer` countdown), `DispatchStoppageEventTriggered` (the full
gameState dispatch: 21/22/25/26/27/28/29/30/24 each to their named
transition, plus the "plain stoppage" fallthrough that arms the
break-camera ladder and the `ST_KEEPER_HOLDS_THE_BALL` clock-panel
special-case), `DispatchBreakCameraMode` and all nine modes (0-8) of the
break-camera ladder, `DoGoalkeeperSprites` (the keeper-dive ball-pinning
Z-table lookup, including the task-#186 bug-fix note on selecting the
keeper sprite by team*number*, not physical end), `MarkPlayer`,
`SetCameraMovingToShowerState`/`FirstHalfJustEnded`/`GoToHalftime`/
`GameOver`, `IsMatchRunning`/`SetMatchRunning`, and the four FSM-interval
setters.

**Where a "stub" function also had a real `Memory` side-effect, only the
audio/render half was omitted, not the whole function** (same discipline
as every prior step, worth restating here since this file has several):
`StubLoadCrowdChantSampleIfNeeded`'s one-shot flag clear, `StubHandlePauseAndStats`'s
`statsEnqueued` clear, and `PlayEnqueuedSamples`'s `goalCounter--`
(read by `GameTime.cs`'s period-end gate) are all real writes, ported;
only the paired `MatchAudio.*` calls are omitted. `StubHandleKeys` is the
one function that really is a total no-op even in the C# (confirmed by
reading it) — omitted entirely, not just its audio half.

**One new real dependency, forward-pulled as a minimal extension:**
`FirstHalfJustEnded` calls `PlayerEnergy.RecoverAtHalfTime()` — not
previously pulled into the step 5/5.5/7A minimal `PlayerEnergy` slice
(those needed `EffectEnabled`/`ShotPenalty`/`SpeedStep`/`DrainOnKeeperCatch`/
`KeeperSkillPenalty`/`DrainSlot`/`DrainOnTackle`/`InjuryRiskDoubled` only).
Added as `swosPlayerEnergyRecoverAtHalfTime()`, extending
`swos_player_energy.h`/`.c` rather than creating a second file — recovers
40% of each player's lost energy, unconditionally (not gated on
`EffectEnabled`, same as `DrainSlot`).

**Differential tests, full VM/Memory state:**
`tools/csharp-golden-dump/Step11GameLoopGolden.cs`, 48 scenarios — the
top-level entry points, every branch of
`UpdateGameTimersAndCameraBreakMode` (interval-seed correction, in-progress
fast path, the waiting-on-player accumulator's human/CPU-not-yet/CPU-kicks/
safety-net-fires branches, all four fire-fast-forward ceremony states, the
stoppage-timer countdown), every one of `DispatchStoppageEventTriggered`'s
ten named `gameState` arms plus the plain-stoppage fallthrough (both with
and without the keeper-holds clock-panel special case), the
`DispatchBreakCameraMode` guard, and all nine break-camera-ladder modes
(including mode 7's three branches: waiting for a controlled player,
arming the result panel once one appears, and the timeout fallback to
`CheckIfGoalkeeperClaimedTheBall`), the four half-end/game-over
transitions (`FirstHalfJustEnded` verified against real energy recovery),
`IsMatchRunning`/`SetMatchRunning`, and the interval setters.
`tests/test_step11_gameloop_golden.c` replays each scenario through the C
port and byte-compares the full `0x60000` buffer.

**48/48 match byte-for-byte, first run.** `make test` (sixteen suites):
**485/485** pass (26 + 44 + 2 + 40 + 25 + 31 + 45 + 12 + 24 + 11 + 23 + 27 +
59 + 40 + 28 + 48). ARM9/BlocksDS cross-compile: clean, zero warnings,
`.nds` built successfully (build-only check).

**Step 11 is now fully complete** — `SetPieces`/`GameTime`/`Referee`/
`Result` (step 10), the six smaller `GameLoop.cs` dependencies (11A),
`Bench.cs`'s dedicated full port, and `GameLoop.cs` itself (11B) all land
in this session. This is the first point where the complete per-tick
match-simulation pipeline — clock, set pieces, referee, scorer list, camera,
substitutions, and the full stoppage/restart orchestration — runs
byte-exact against the real OpenSWOS C#.

### Status: step 12 (2026-09-16) — DS renderer adapter, first playable milestone

Different in kind from steps 1-11: there is no more C# to mechanically port
from, so this step is a hand-designed adapter, not a verified translation.
New standalone project `nds-app/` (own `Makefile`, modeled on
`nds-checkpoint/Makefile` + `../swos-ds/Makefile`'s `GFXDIRS` asset
pipeline) builds the real ported VM (`../src`, `../include`, unmodified —
same sources the desktop tests use) together with new DS-adapter-only code
under `nds-app/source/`:

- `match_bootstrap.c/.h` — seeds a placeholder 11-a-side roster (flat
  mid-range skills, `PlayerInfo` records poked directly into Memory —
  explicitly supported by `swos_team_data_loader.h`'s own comment: "works
  correctly against a `PlayerInfo` block populated by any means") at a
  hand-picked 1-4-4-2 formation on `swos-ds`'s known 672x848 pitch, wires
  `TeamData`/`topTeamInGame`/`bottomTeamInGame`, calls the real
  `swosKickoffPrepareForInitialKick()` + `swosCameraSetToInitialPosition()`,
  then forces `gameStatePl` straight to `K_ST_GAME_IN_PROGRESS` (skipping
  the referee whistle/waiting-on-player handshake, which would need
  `Main.cs`-level orchestration never ported here). Both teams start
  AI-controlled (`TEAMDATA_OFF_PLAYER_NUMBER = 0`) — matches the plan to
  get AI-vs-AI on screen first; flipping one team to human control is a
  small follow-up (`swosInputControlsSetJoystickState()` already exists,
  ported in step 8).
- `player_anim.c/.h` — real running/standing animation-frame tables (local
  atlas indices, `local = global - 341`), copied verbatim from
  `../../swos-ds/source/player.c` — genuine extracted data, not fabricated.
- `main.c` — GL2D setup, per-frame `scanKeys()` → `swosGameLoopTick()` →
  render, all copied/adapted from `../../swos-ds/source/main.c`'s
  already-proven pattern. Renders the pitch tilemap, the ball (with a
  height-lifted sprite using the real `Z` field), and all 22 player
  sprites (position/direction read straight from `PlayerSprite` Memory
  fields via the existing ported accessors). Uses its own simple
  follow-the-ball scroll camera (not the ported `swos_camera.c` state,
  whose clipping convention against a 256x192 DS screen wasn't verified —
  `swos_camera.c` still runs every tick as part of the real simulation,
  its output just isn't used for the on-screen scroll offset).
- Graphics assets (`nds-app/graphics/*.png`+`.grit`,
  `nds-app/source/player_atlas.*`/`ball_atlas.*`/`player_frame_centers.*`/
  `pitch_map.h`) are copies of files `swos-ds` already extracted from the
  user's legally-owned GOG SWOS install. `swos-ds` itself is untouched —
  copying data files for reuse, never editing the sibling project.

**Deliberately NOT mechanical-port fidelity** — this whole step has no
OpenSWOS source to verify against, unlike every prior step. The formation
coordinates are hand-picked (not OpenSWOS's own
`kTopStartingPositions`/`kBottomStartingPositions` tables — their exact
coordinate-scale convention wasn't verified against `swos-ds`'s
`WORLD_W`/`WORLD_H`, and getting it subtly wrong would look worse than an
honest placeholder), and the kickoff-whistle handshake is skipped outright.

ARM9/BlocksDS build (`nds-app/`): clean, zero warnings, `swos_vm_ds_app.nds`
built successfully. Desktop `make test` (unaffected — no `src`/`include`
changes this step): still 485/485. No DS emulator is available in this
environment (see `feedback_host_gcc_devkitpro_mingw64.md`) — the build is
verified compile-clean but not yet visually confirmed on real hardware or
an emulator; that's the next thing to actually try.

## Porting order (full plan, revised 2026-09-16 after step 4's file-graph discovery)

1. ~~Memory, types, CPU flags, tables, RNG~~ (2026-09-15, see Status above)
2. ~~Sprite views: `BallSprite`, `PlayerSprite`, `TeamData`~~ (2026-09-15, see Status above)
   - ~~2.5: `AnimationTablesData` + full `Memory.Init()`, golden-dump verified~~ (2026-09-15, see Status above)
3. ~~`SpriteUpdate`~~ (2026-09-16, see Status above)
4. ~~`BallUpdate`~~ (2026-09-16, see Status above) — plus an ARM/BlocksDS
   checkpoint (`nds-checkpoint/`, see its own status entry below) — pulled
   forward and fully ported alongside `BallUpdate.cs` itself:
   `UpdateBallWithControllingGoalkeeper` (one function from
   `PlayerUpdate.cs`), `BallOutOfPlay.cs` (whole file), `UpdateGoals.cs`
   (whole file, minus the deferred `RegisterScorer` call)
5. ~~`PlayerActions.cs`~~ (2026-09-16, see Status above) — the rest of it
   (`SetPlayerAnimationTable` was already done, step 3) — forward-pulled
   minimal slices: `TeamDataLoader`'s `PlayerInfo` offset constants,
   `PlayerEnergy`'s `EffectEnabled`/`ShotPenalty`/`SpeedStep`
5.5. ~~`PlayerUpdate.cs`~~ (2026-09-16, see Status above) — the rest of it
   (`UpdateBallWithControllingGoalkeeper` was already done, step 4) —
   forward-pulled minimal slices: `TeamPort.StopAllPlayers`, the whole
   (tiny) `PortPlayerState` enum, plus extensions to step 5's
   `TeamDataLoader`/`PlayerEnergy` slices.
6. `PlayerControlled`
   - ~~6A: human-team control/pass paths + real Header/Tackle slices~~
     (2026-09-16, see Status above)
   - ~~6B: CPU-only AI hooks — completed with step 9, without a temporary
     no-op~~ (2026-09-16, see Status above)
7. `UpdatePlayers`
   - ~~7A: real local dependencies -- `BallVariables.cs` (full), `TeamPort`/
     `PlayerEnergy` extensions, and the rest of `PlayerHeader.cs`/
     `PlayerTackle.cs` (both now fully ported)~~ (2026-09-16, see Status
     above)
   - ~~7B: `UpdatePlayers.cs` itself (4349 lines) — AI branches assert to
     step 9 (reusing 6A's hook globals); the two real `SetPieces` calls
     this file makes get a new assert-backed hook pair, replaced in step
     10 — never a silent no-op~~ (2026-09-16, see Status above)
8. ~~`InputControls`~~ (2026-09-16, see Status above)
9. ~~`AiHelpers`, `AiBrain`~~ (2026-09-16, see Status above)
10. ~~`SetPieces`, `GameTime`, `Referee`, plus `Result.cs`~~ (2026-09-16,
    see Status above) -- `GameTime.cs` also unblocked `Result.RegisterScorer`'s
    `PORT_PENDING` hook (step 4's `swosRegisterScorerHook`), now wired to
    the real `Result.cs` port. Forward-pulled minimal slices:
    `GameLoop.PlayersLeavingPitch` (`swos_game_loop.h`/`.c`),
    `BallSim.CurrentPitchType` (a plain C global in `swos_game_time.c`).
11. ~~`GameLoop` orchestration~~ (2026-09-16, see "Status: step 11B" above)
    -- `PlayersLeavingPitch` already forward-pulled (step 10). Split (like
    steps 7A/7B) after its own dependency scan found ~3900 more lines
    across 7 new files:
    - ~~11A: real local dependencies -- `Kickoff.cs` (minimal slice),
      `Camera.cs`, `GameSprites.cs`, `SpinningLogo.cs`,
      `PlayerNameDisplay.cs`, `Stats.cs` (all full ports), plus a minimal
      `Bench.cs` extension (`InBenchMenus`/`GetBenchState`)~~ (2026-09-16,
      see Status above)
    - ~~`Bench.cs` full port (1868 lines -- `UpdateBench`/
      `CheckIfGoalkeeperClaimedTheBall` reach almost the entire file, not a
      minimal-slice candidate)~~ (2026-09-16, see "Status: step 11" above)
    - ~~11B: `GameLoop.cs` itself (2045 lines)~~ (2026-09-16, see "Status:
      step 11B" above)
12. ~~Adapter from VM state to the DS renderer~~ (2026-09-16, see "Status:
    step 12" above) — first playable milestone (`nds-app/`, AI vs AI); not
    mechanical-port fidelity like steps 1-11 (no C# source to verify
    against), see its own status entry for what's hand-designed vs. real.

After each module: desktop build + tests, diff against OpenSWOS behavior
where practical, periodic `.nds` build once there's something to render.
