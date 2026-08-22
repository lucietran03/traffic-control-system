# System Diagrams (ASCII)

Companion to `PROJECT_SPECIFICATION.md`. All diagrams use **plain ASCII characters** (`=`, `-`, `|`, `+`, `#`) rather than Unicode box-drawing/arrow characters (═, │, ▓, →...) — several Unicode box-drawing and arrow characters render *double-width* on some fonts/viewers, which silently breaks column alignment even when the source is correctly spaced. Plain ASCII renders single-width everywhere.

Naming reminder: `I1–I6` / `RC1–RC3` are **physical** locations; `L1–L6` / `RL1–RL3` are the **controllers** that own them; `C1` is the Central Controller.

---

## Diagram 1 — Network Overview

```text
LEGEND
  ====   Arterial road (R1 or R2), running West-East
  |      Connector road (R3, R4, or R5), running North-South
  ####   Double-track railway corridor, running West-East
  [In]   Signalised intersection n  (controller: Ln)
  (RCn)  Railway level crossing n   (controller: RLn)


                                    NORTH

           R1
      ==[I1]============[I3]============[I5]==
          |                 |                |
          | R3              | R4             | R5
          |                 |                |
      ##########################################
        (RC1/RL1)       (RC2/RL2)        (RC3/RL3)
          |                 |                |
          |                 |                |
      ==[I2]============[I4]============[I6]==
           R2

                                    SOUTH
```

**Reading the diagram:** travelling North to South down the left column encounters `I1` then `I2` (linked by `R3`, crossing the railway at `RC1`) — the Pass-minimum slice. The middle column is `I3`/`I4` (linked by `R4`, crossing at `RC2`) — the committed HD extension. The right column is `I5`/`I6` (linked by `R5`, crossing at `RC3`) — optional/stretch scope. The two horizontal rows, `R1` (North) and `R2` (South), are the two arterials, each linking three intersections in a row — these are the roads used for green-wave coordination (Section 20 of the specification).

---

## Diagram 2 — Core Lane Layout (2 lanes, permissive, right-hand traffic)

```text
Right-hand traffic (Vietnam convention): for each direction of travel, the
median is always on the LEFT of the driver, and the kerb is always on the
RIGHT. Applies to every road in the network (arterial R1/R2 and connector
R3/R4/R5 alike) — same lane rule everywhere, only speed/priority differ.

Example cross-section of connector road R3 (North-South, between I1 to
the North and I2 to the South):

NORTHBOUND direction (toward I1):
  - Median-side lane : through + LEFT turn (permissive - yields to the
    opposing through movement)
  - Kerb-side lane   : through + RIGHT turn (permissive - no turn on red)

-------------------- median (centre line) --------------------

SOUTHBOUND direction (toward I2):
  - Median-side lane : through + LEFT turn (permissive)
  - Kerb-side lane   : through + RIGHT turn (permissive)

CORE RULE (every approach, every intersection):
  - Median-side lane = through + permissive left turn
  - Kerb-side lane   = through + permissive right turn
  - One circular signal head per approach - NO dedicated arrow signal
  - See PROJECT_SPECIFICATION.md, Section 5b, for the documented
    alternative (dedicated left/through/right lanes with protected
    turn arrows) that was evaluated but deferred to a future iteration
```

---

## Diagram 3 — Intersection Component Layout (generic `Ix` / `Lx`)

```text
LEGEND
  [V]  Vehicle signal head          [Ps] Pedestrian signal (per side)
  [Pb] Pedestrian push-button       [S]  Vehicle presence sensor (at stop line)
  [Q]  Advance/queue sensor (only on an approach feeding a railway crossing)

                        NORTH approach
                  [V][S]     [Pb][Ps]


  WEST approach                                   EAST approach
  [V][S]                                              [S][V]
  [Pb][Ps]        +-----------------+          [Pb][Ps]
                   |                 |
                   |       Lx        |<--- status/commands --->  C1
                   |                 |
                   +-----------------+


                  [V][S]     [Pb][Ps]
                  [Q]  (present only if this approach feeds a
                        railway crossing, e.g. I1's South approach
                        toward RC1)
                        SOUTH approach
```

`Lx` owns and controls every device shown at all four approaches. If an approach leads to a railway crossing, `Lx` additionally receives status (one-way, read-only) from that crossing's `RLx` — see Diagram 4.

---

## Diagram 4 — Railway Crossing + Two Adjacent Intersections (generic `RCx` / `RLx`)

Shown for `RC1`, on connector road `R3` between `I1` (North, controller `L1`) and `I2` (South, controller `L2`) — the same layout applies to `RC2` (between `I3`/`I4`) and `RC3` (between `I5`/`I6`).

```text
LEGEND
  [FL]  Road-facing flashing lights     [BG]   Boom gate arm
  [TA]  Train approach sensor           [BgS]  Boom-gate position sensor
  [TS]  Train signal (one per rail direction)
  [Ex]  Exit sensor - RESERVED, not used in Core logic (Section 9, item 5)

     I1 (L1) - North
        |
        |   R3 - connector road, 2 lanes each direction
        |
  [TA-A]-+-[BG-North][BgS-North][FL-North][TS-A]   <- rail direction A
        |
   ##########  RC1 - double-track railway corridor  ##########
        |
  [TA-B]-+-[BG-South][BgS-South][FL-South][TS-B]   <- rail direction B
        |
     I2 (L2) - South


              +--------+
              |  RL1   |-------- status/faults -------->  C1
              +--------+
       owns and actuates: [BG], [FL], [TS] on both directions
       sends crossing state (READ-ONLY, one-way) to both L1 and L2
```

Safety rules illustrated here (see `PROJECT_SPECIFICATION.md`, Section 22):

- `RL1` is the **only** controller allowed to actuate `[BG]`, `[FL]`, or `[TS]`.
- `L1`/`L2` may only **read** `RC1`'s state — never command it.
- `[TS]` shows `PROCEED` only once both `[BgS]` confirm the gates are fully `DOWN`.
- `[Ex]` exists physically but is not wired into the reopening decision — the Core design uses a fixed occupancy-window timer instead (Section 14).
