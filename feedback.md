# Proposed Revision Plan — Section 4.1 State Charts

**Status:** Awaiting team approval. No change described below should be applied to the report files until this plan is approved.

## 1. Files in Scope

If approved, the revision will update only:

- `system_assumptions_tables.md` — add the missing override assumption;
- `STATE_CHARTS.md` — correct the state-chart logic and handoffs;
- `SEQUENCE_DIAGRAMS.md` — restore the matching override-renewal interaction;
- `feedback.md` — replace this plan with a concise completion record after validation.

No unrelated report section, source file, or untracked file will be changed.

## 2. Proposed Changes

### 2.1 Add the missing bounded-override assumption

Add a new assumption after `PA-10` and number it `PA-11`. It will state that:

- a `CLEAR_ROUTE` override has a finite requested duration;
- the accepted duration is capped at 5 minutes;
- the override expires automatically unless a valid renewal is accepted;
- the operator may cancel it earlier;
- every initial request and renewal remains subject to local safety validation.

The exact wording will be concise and will distinguish this team design decision from requirements originating in the official project brief. Existing references following `PA-10` will be checked and renumbered only if required.

### 2.2 Correct acknowledgement handling in SC-03B

Revise the transition into `OVERRIDE_PENDING` so an accepted request receives an immediate acknowledgement instead of remaining unacknowledged while pedestrian clearance completes:

```text
/ ACK(accepted, pending clearance), queue request
```

When the pedestrian clearance completes, the controller will revalidate the request, apply it only if it remains safe, and report the resulting status. It will not issue the initial ACK for a second time.

This preserves both rules:

- every Central command receives a timely `ACK` or `NACK` (`PA-09`);
- an active pedestrian clearance is never truncated (`TL-05`, UC-08).

### 2.3 Validate override renewals in SC-03B

Replace the unconditional `ACTIVE → ACTIVE` renewal self-loop with explicit validation:

- a valid, bounded renewal receives `ACK` and restarts the timer;
- an invalid, unsafe, or over-limit renewal receives `NACK` and does not extend the current expiry time;
- railway pre-emption or a local fault still interrupts the override according to the priority rules in SC-03A.

SC-03B's description, guards, actions, and notes will cite the new `PA-11` rather than the internal `PROJECT_SPECIFICATION.md`.

### 2.4 Align SD-07 acknowledgement and renewal behaviour

Retain the renewal branch that now exists in `SD-07`, but correct its acknowledgement and validation order so it matches SC-03B:

1. an initial request that is safe but waiting for pedestrian clearance receives `ACK(accepted, pending clearance)` promptly rather than remaining unacknowledged;
2. after clearance completes, `Lx` revalidates the retained request before applying it;
3. a renewal request is revalidated against its duration and current safety conditions;
4. a valid renewal receives `ACK` and restarts the timer;
5. an invalid renewal receives `NACK` and leaves the existing expiry unchanged;
6. expiry, cancellation, railway pre-emption, or a local fault terminates the override safely.

The sequence diagram will cite `PA-11` so its behaviour matches SC-03B and UC-08. The in-diagram traceability-gap note will be removed once `PA-11` exists.

### 2.5 Make the SC-01 sub-diagram handoffs explicit

Keep the `SC-01A`/`SC-01B`/`SC-01C` split for A4 legibility, but replace note-only handoffs with modelled exits:

- `SC-01B` and `SC-01C` will show a pending mode change being evaluated only at a safe phase boundary;
- when the selected mode changes, the detail chart exits through a named handoff state back to `SC-01A`;
- when the selected mode is unchanged, the current phase cycle continues;
- no transition will imply a mid-green or mid-clearance mode change.

The notes that merely repeat this behaviour will then be shortened or removed.

### 2.6 Complete the SC-04B re-closing path

Remove the dead-end `CLOSING_REF` state. If a train is detected while the gates are opening, SC-04B will explicitly:

1. stop the opening operation;
2. keep or reactivate the flashing warning;
3. register the new occupancy window;
4. command both gates down;
5. require gate-closed confirmation again;
6. return to `CLOSED` only after confirmation, or enter `FAULT` if confirmation is missing or contradictory.

This keeps the split between SC-04A and SC-04B while ensuring every state has a complete and visible continuation.

### 2.7 Cover repeated pedestrian presses throughout active service

Update SC-02 so a repeated `PED_REQUEST(side)` is coalesced not only in `REQUEST_LATCHED`, but also while `WALK` or `FLASHING_DONT_WALK` is active. No repeated press will create a second queued service request during the same pedestrian-service lifecycle (`TL-06`).

The existing notes about railway-delayed service (`PA-02`) and permanently silent buttons (`PA-03`) will remain because they document limitations that cannot be represented as detectable transitions. Their wording will not claim that the official brief requires Mermaid notes.

### 2.8 Preserve useful notes without overcrowding the figures

Retain only notes that communicate a non-obvious constraint or accepted limitation, including:

- the approximately 45-second value is a total warning-to-arrival budget (`RC-03`);
- permanently silent pedestrian and train-approach inputs are not detectable in the Core design (`PA-03`, `RC-11`);
- Central-link loss alone does not force all-red or `FLASHING_RED` (`PA-07`).

Move or remove notes that merely repeat visible transitions or prose already stated immediately above a diagram.

## 3. Items That Will Not Change

- The design will retain five logical state-chart families: `SC-01`–`SC-05`.
- The lettered A/B/C sub-diagrams will remain where required for A4 readability.
- `L1–L6` will continue using one generic intersection-controller model.
- `RL1–RL3` will continue using one generic railway-controller model.
- Central will not gain direct authority over traffic lights, boom gates, flashing lights, or train signals.
- Railway protection and local fault handling will continue to take priority over Central overrides.
- The Core design will continue using timer-based railway occupancy windows rather than exit-sensor confirmation.

## 4. Validation Required Before Completion

After the approved changes are implemented:

1. Render every Mermaid block in `STATE_CHARTS.md` and `SEQUENCE_DIAGRAMS.md` using a real Mermaid renderer.
2. Confirm there are no parse errors, unintended states created from prose, dead-end states, or unreadable cross-diagram connectors.
3. Verify every state has a defined continuation or an intentional terminal outcome.
4. Verify SC-03B and SD-07 use the same ACK/NACK, pending, renewal, expiry, cancellation, railway-interruption, and fault-interruption rules.
5. Verify all numeric and behavioural team decisions cite finalized assumption IDs, including `PA-11`.
6. Verify no report-facing text cites `PROJECT_SPECIFICATION.md` as an official authority.
7. Verify the diagrams remain readable when exported for an A4 report page.
8. Run Markdown and Git diff checks and confirm that no unrelated file is included in the commit.

## 5. Approval Gate

Implementation will begin only after the team explicitly approves this plan. Any requested change to the plan will be incorporated into this file before the report files are edited.
