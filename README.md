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
- `tools/convert_addr.py`, `tools/extract_table.py` — the generators for the
  two generated headers above. Re-run them if OpenSWOS's `Memory.cs` /
  `Rng.cs` / `Tables.cs` change; never edit their output by hand.
- `tests/test_memory.c` — desktop smoke test (new code, not a port of
  anything) that cross-checks the ported layer against the literal comments
  in the C# source.

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

If OpenSWOS and `swos-port` (see `../swos-port`, checked out as reference in
the sibling project) disagree, `swos-port`/the ASM is the source of truth,
not the C#.

## Status (2026-09-15)

Step 1 of the porting order (Memory / Flags / Rng / Tables) is **partially**
done:

| Module | State |
|---|---|
| `swos_addr.h` (`Memory.Addr`) | Done — generated, 384/384 constants |
| `swos_tables_data.h` (`Rng.kRandomTable`, `Tables.k*`) | Done — generated |
| `swos_flags.{h,c}` (`Flags.cs`) | Done — full hand port |
| `swos_rng.{h,c}` (`Rng.cs`) | Done — full hand port (both streams) |
| `swos_memory.{h,c}` Read/Write helpers | Done — full hand port |
| `swos_memory` `Init()` (`Memory.cs:1477-2271`, ~800 lines) | **Not ported.** It calls `PlayerSprite.Init()` / `AnimationTablesData.Init()` / `TeamData.Init()`, none of which exist here yet (those are step 2). Porting it now would mean stubbing those three calls, which would misrepresent this layer as more complete than it is. `swosMemoryInitStub()` (zero-fill only) stands in for now. |

`make test` builds and runs the smoke test (desktop-only; see Makefile
comment for why it pins devkitPro's bundled mingw64 gcc as the *host*
compiler — unrelated to the project's actual ARM/BlocksDS target toolchain).

## Porting order (full plan)

1. ~~Memory, types, CPU flags, tables, RNG~~ (this session, see Status above)
2. Sprite views: `BallSprite`, `PlayerSprite`, `TeamData`
3. `SpriteUpdate`
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
