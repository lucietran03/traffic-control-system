# System Diagrams (ASCII)

Companion to `PROJECT_SPECIFICATION.md`. All diagrams here are **schematic**, not to scale, and drawn in plain text/ASCII deliberately — this avoids any copyright concern with reusing a real map image, and keeps every diagram diffable/editable in the repository like code. They are safe to paste directly into the Initial Design Report as a starting point for the formal UML/diagramming-tool versions.

Naming reminder (see `PROJECT_SPECIFICATION.md` Section "Naming convention"): `I1–I6` / `RC1–RC3` are **physical** locations; `L1–L6` / `RL1–RL3` are the **controllers** that own them; `C1` is the Central Controller.

---

## Diagram 1 — Network Overview (roads, railway, intersections, legend)

```text
LEGEND
  ══════════   Arterial road (R1 or R2) — 2 lanes each direction
  ┄┄┄┄┄┄┄┄┄┄   Connector road (R3, R4, or R5) — 2 lanes each direction
  ▓▓▓▓▓▓▓▓▓▓   Double-track railway corridor
  [ In ]       Signalised road intersection n  (controller: Ln)
  ( RCn )      Railway level crossing n        (controller: RLn)
  C1           Central Controller — control room (logical, not on this map)


                    R1 (arterial, north-south)      R2 (arterial, north-south)
                              ║                                ║
                              ║                                ║
   [ I5 ]═══════════════════ ╬ ══════════ R5 ══════════════════╬ ═══════════════[ I6 ]
   (L5)                      ║        ▓▓( RC3 / RL3 )▓▓        ║                (L6)
                              ║                                ║
                              ║                                ║
                              ║                                ║
   [ I3 ]═══════════════════ ╬ ══════════ R4 ══════════════════╬ ═══════════════[ I4 ]
   (L3)                      ║        ▓▓( RC2 / RL2 )▓▓        ║                (L4)
                              ║                                ║
                              ║                                ║
                              ║                                ║
   [ I1 ]═══════════════════ ╬ ══════════ R3 ══════════════════╬ ═══════════════[ I2 ]
   (L1)                      ║        ▓▓( RC1 / RL1 )▓▓        ║                (L2)
                              ║                                ║


NOTE ON THE ORIGINAL BRIEF DIAGRAM:
The official project diagram draws the railway corridor as one continuous
diagonal line crossing the whole network. This schematic "straightens" that
diagonal into three separate crossing points (RC1, RC2, RC3) — one per
connector road — because that is the functional relationship that actually
matters for control logic (which controllers see which crossing's status).
No topology has been changed; this is purely a clearer way to draw the same
relationships described in Section 5 of PROJECT_SPECIFICATION.md.

Pass-minimum PoC slice = the bottom row only: I1, I2, R3, RC1/RL1, plus C1.
Committed HD-extension = bottom row + middle row: adds I3, I4, R4, RC2/RL2.
```

---

## Diagram 2 — Road Lanes and Allowed Directions

Applies to every road in the network (arterial `R1`/`R2` and connector `R3`/`R4`/`R5` alike) — same lane rule everywhere, only the speed/priority differs (Section 5, Section 12 of the specification). Shown as the view looking along a road segment toward an intersection:

```text
LEGEND
  ↑ / ↓   Through movement
  ↶       Left turn (permissive — yields to oncoming traffic, no arrow)
  ↷       Right turn (permissive — no turn against a red signal)

                              MEDIAN / CENTRE LINE
   ══════════════════════════════╪══════════════════════════════
   NORTHBOUND SIDE (2 lanes)     │      SOUTHBOUND SIDE (2 lanes)
   ── toward the intersection    │      ── away from the intersection
                                  │
   ┌───────────────────────────┐ │ ┌───────────────────────────┐
   │ Inner lane (nearest median)│ │ │ Outer lane (nearest kerb)  │
   │   ↑ Through   ↶ Left       │ │ │   ↓ Through   ↷ Right      │
   ├───────────────────────────┤ │ ├───────────────────────────┤
   │ Outer lane (nearest kerb) │ │ │ Inner lane (nearest median)│
   │   ↑ Through   ↷ Right      │ │ │   ↓ Through   ↶ Left       │
   └───────────────────────────┘ │ └───────────────────────────┘
                                  │
   ══════════════════════════════╪══════════════════════════════

RULE (applies to every approach, every intersection):
  - Inner lane  = Through + Left  (permissive left, shares the through phase)
  - Outer lane  = Through + Right (permissive right, shares the through phase)
  - No dedicated turn-arrow phase in the Core design (Section 23 — Rejected)
  - No turn permitted against a red signal in either lane
```

---

## Diagram 3 — Intersection Component Layout (generic `Ix` / `Lx`)

Shown for a 4-way intersection. The **advance/queue sensor** on the South approach is only present at the two intersections whose connector road leads to a railway crossing (e.g. `I1`, `I2` for `RC1`) — see Section 9, item 2 of the specification.

```text
LEGEND
  [V]  Vehicle signal head          [Ps] Pedestrian signal (per side)
  [Pb] Pedestrian push-button       [S]  Approach vehicle presence sensor
  [Q]  Advance/queue sensor (HD; only on crossing-facing approaches)

                                NORTH APPROACH
                        [V]  [S]        [Pb][Ps] (N crossing)
                                   │
   WEST APPROACH                  │                  EAST APPROACH
   [V] [S]                        │                        [S] [V]
   (W crossing) [Pb][Ps] ──────┐  │  ┌────── [Pb][Ps] (E crossing)
                                │  │  │
                        ────────┤  │  ├────────
                                │  │  │
                            ┌───┴──┴──┴───┐
                            │             │
                C1 ◄───────►│   Lx        │  local controller process
          (status/commands) │ (this node) │  — owns every device shown here
                            │             │
                            └───┬──┴──┬───┘
                                │  │  │
                        ────────┤  │  ├────────
                                   │
                        [Pb][Ps] (S crossing)
                        [V]  [S]
                        [Q]  ← present only if this approach feeds a
                               railway crossing (e.g. I1's South approach → RC1)
                                SOUTH APPROACH
                          (toward railway crossing, where applicable)
```

---

## Diagram 4 — Railway Crossing + Adjacent Intersections (generic `RCx` / `RLx`)

Shown for `RC1`, sitting on connector road `R3` between `I1` and `I2` — the same layout applies to `RC2` (between `I3`/`I4`) and `RC3` (between `I5`/`I6`).

```text
LEGEND
  [FL]  Road-facing flashing light set     [BG]  Boom gate arm
  [TS]  Train signal (one per rail dir.)   [TA]  Train approach sensor
  [BgS] Boom-gate position sensor          [Ex]  Exit sensor — RESERVED,
                                                   not used in Core logic
                                                   (Section 9, item 5)

  I1 (L1)                                                        I2 (L2)
    │                                                                │
    │◄─────────────── R3 connector road, 2 lanes each way ─────────►│
    │                                                                │
    │        ▓▓▓▓▓▓▓▓▓▓▓▓▓▓  RAILWAY CORRIDOR  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓        │
    │        ▓▓▓▓▓▓▓▓▓▓▓▓▓▓   (double track)    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓        │
    │                                                                │
    │   Direction A ──►                              ◄── Direction B │
    │   [TA-A]  [BG-W][BgS-W]   RC1   [BG-E][BgS-E]  [TA-B]           │
    │   ([Ex-A] reserved)                          ([Ex-B] reserved) │
    │   [FL-W]                                              [FL-E]  │
    │   [TS-A]                                              [TS-B]  │
    │                                                                │
    │                             ┌────────────┐                    │
    │                             │    RL1     │◄── faults/status ─►│  C1
    │                             │ (this node)│                    │
    │                             └─────┬──────┘                    │
    │                                   │                            │
    │            crossing state (status ONLY, one-way, no commands) │
    │  ◄────────────────────────────────┴───────────────────────────►
    │  L1 receives RC1 status                    L2 receives RC1 status

KEY RULES SHOWN HERE (see Safety Invariants, Section 22):
  - RL1 is the ONLY controller that may move [BG], [FL], or [TS].
  - L1 and L2 may only READ RC1's crossing state — never command it.
  - [TS] shows PROCEED only if both [BgS] confirm gates fully DOWN.
  - [Ex] sensors exist physically but are not wired into the reopening
    decision today — reopening uses a fixed occupancy-duration timer
    (Section 14). They are reserved for a possible future enhancement.
```
