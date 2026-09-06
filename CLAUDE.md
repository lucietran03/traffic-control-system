I need you to review and update KAN-52 in my PERSONAL Jira project KAN.

KAN-52 is currently:

“RTS-2B Validate Shared QNX Build & IPC Contract + M3 Readiness Sync”

Before editing the Jira issue, inspect the actual current implementation in this repository:

https://github.com/lucietran03/traffic-control-system/tree/main/app

IMPORTANT:
Do not treat this as a code review only, and do not treat the meeting as a technical knowledge test.

KAN-52 is a PROJECT READINESS MEETING.

KAN-12/M2 is where the team agrees on:

- project scope
- architecture
- controller/process/node mapping
- IPC contract
- ownership
- integration sequence
- repository/team workflow
- dependencies and blockers

KAN-52/M3 should then check the REAL repository against that agreement and determine whether the shared foundation and the TEAM are ready to move into parallel feature implementation.

STEP 1 — INSPECT BEFORE EDITING

Inspect the repository, especially the /app implementation, and determine what actually exists now.

Check, where supported by the repository:

- project/app structure
- processes/executables currently implemented
- controller/component boundaries
- shared headers/types/configuration
- QNX-specific code
- name_attach()/name_open() usage
- MsgSend()/MsgReceive()/MsgReply() usage
- pulses if present
- service/channel naming
- message/request/reply structures
- process discovery
- Qnet/cross-node assumptions or implementation
- error handling
- unavailable-peer handling
- timeout behaviour
- invalid/rejected-message handling
- build/run configuration
- any README or documentation relevant to running the shared baseline

Also inspect Git/repository structure enough to understand whether three members can reasonably work against the same baseline.

Do NOT assume something exists simply because KAN-12 planned it.

Clearly distinguish:

1. already implemented
2. partially implemented
3. planned but not implemented
4. cannot be verified from the repository

STEP 2 — COMPARE AGAINST KAN-12

Read KAN-12 in Jira before rewriting KAN-52.

Treat KAN-12 as the agreed project/team baseline and the repository as the current implementation state.

Identify discrepancies such as:

- planned process/interface not present in code
- different message structure
- different naming
- different controller allocation
- missing shared configuration
- undocumented implementation decision
- implementation that has progressed beyond KAN-12

Do not automatically change KAN-12.

KAN-52 should capture what needs to be validated or reconciled before feature work continues.

STEP 3 — CHECK PROJECT READINESS, NOT ONLY TECHNICAL VALIDATION

KAN-52 must cover two dimensions:

A. TECHNICAL BASELINE READINESS

Determine what the team actually needs to validate together, based on the repository.

This may include:

- shared build
- process startup
- discovery
- request/reply IPC
- cross-node Qnet communication where actually required
- shared message contract
- failure/error behaviour
- reproducibility on each member's environment

Do not invent validation steps that are irrelevant to the current architecture.

B. TEAM EXECUTION READINESS

The meeting must also confirm:

- all three members can access and build/use the shared baseline
- each member knows their immediate implementation responsibility
- component ownership is still correct
- interface ownership is clear
- dependencies between Lucie, Hưng and Thắng are understood
- branch/merge workflow is usable
- shared files/interfaces have clear ownership/change rules
- integration responsibility is agreed
- members know who debugs which side when an interface fails
- evidence/logging conventions are agreed
- blockers have owners
- blockers that prevent another member from starting are prioritised
- the next implementation milestone/checkpoint is agreed

This is NOT an oral QNX test.

Do not write acceptance criteria such as every member having to explain arbitrary QNX theory.

The standard should instead be:
each member has sufficient access, environment setup, project understanding, and shared technical baseline to begin their assigned implementation without unnecessary dependency on another member.

STEP 4 — REWRITE KAN-52

After the inspection, rewrite KAN-52 using this structure:

Title
Purpose
Current Repository Baseline
What to Validate Before/During M3
Technical Validation
Team Readiness & Ownership Check
Deliverable
Quality Bar
Acceptance Criteria
Evidence to Keep
Next Step

The “Current Repository Baseline” section MUST be based on what you actually found in the repository.

Do not claim a QNX behaviour, interface, process, Qnet path, timeout mechanism, or failure-handling mechanism exists unless the repository supports that claim.

For missing functionality, phrase it as something to implement/validate, not as an existing feature.

Keep the ticket practical enough to be used during the actual team meeting.

Avoid turning it into a huge generic checklist.

STEP 5 — JIRA SAFETY

Before updating:

- confirm you are editing KAN-52 in my PERSONAL KAN project
- preserve useful existing content where appropriate
- do not create a duplicate issue
- do not change unrelated RTS issues
- do not mark KAN-52 Done
- keep due date 6-Sep-2026 unless Jira/current project evidence gives a clear reason otherwise

After updating KAN-52, reopen/read it and verify the saved content.

Then report back briefly with:

1. What the repository currently contains
2. Main differences between KAN-12 and implementation
3. What KAN-52 now asks the team to validate
4. What M3 needs to decide about teamwork/ownership
5. Any blocker that should prevent feature implementation from starting
