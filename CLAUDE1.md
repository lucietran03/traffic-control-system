I want to clean up and reorganise this repository because it has become very messy.

The repository currently contains a mixture of:
- current implementation code
- old / legacy code
- build outputs and generated files
- documentation
- specifications
- verification / testing files
- lecture materials
- Vietnamese and English versions of documents
- storyboards
- presentation-related files
- traffic-light related files
- tools and scripts
- temporary files
- duplicate / obsolete files
- files outside the main source-code structure

IMPORTANT:
This must be a TWO-PHASE process with a hard approval gate.

==================================================
PHASE 1 — READ, INVENTORY, CLASSIFY, ASK
==================================================

DO NOT MODIFY ANYTHING IN PHASE 1.

Do NOT:
- delete files
- move files
- rename files
- edit files
- rewrite code
- reorganise directories
- change Git history
- change .gitignore
- run git clean
- run git reset
- remove old code
- overwrite documents

Your only job in Phase 1 is to inspect and analyse.

--------------------------------------------------
1. FULL REPOSITORY INVENTORY
--------------------------------------------------

First inspect the repository structure comprehensively.

Use:
- directory tree
- git status
- git log
- recent commits
- tracked vs untracked files
- file extensions
- file sizes
- directory locations
- references between files where useful

Read the contents of relevant files enough to understand their purpose.

Do NOT assume that a file is unnecessary just because:
- it is old
- it is not imported directly
- it is not currently compiled
- it has a different language
- it is outside the main source directory
- it looks like a duplicate
- it has an old naming convention

Where possible, determine whether a file is:
- actively used
- referenced by another file
- required for build/deployment
- required for testing/verification
- required for documentation/submission
- historical/reference material
- generated
- duplicated
- obsolete
- unclear

--------------------------------------------------
2. PROPOSE A CLEAN CATEGORY STRUCTURE
--------------------------------------------------

Based on what you actually find, propose a sensible categorisation.

Do NOT force every file into a category if the evidence is unclear.

You may use categories such as:

A. CURRENT SOURCE CODE
B. LEGACY / HISTORICAL CODE
C. BUILD / GENERATED FILES
D. CONFIGURATION / ENVIRONMENT
E. TOOLS / SCRIPTS
F. SPECIFICATIONS
G. VERIFICATION / TESTING
H. IMPLEMENTATION DOCUMENTATION
I. LECTURE / REFERENCE MATERIAL
J. STORYBOARD / PRESENTATION / MEDIA
K. VIETNAMESE DOCUMENTS
L. ENGLISH DOCUMENTS
M. ASSETS
N. DUPLICATE / OBSOLETE CANDIDATES
O. UNCLEAR / NEED USER DECISION

However, feel free to propose a better structure if the actual repository suggests one.

The goal is not to maximise the number of folders.

The goal is to make the repository understandable and maintainable.

--------------------------------------------------
3. DETERMINE FILE STATUS
--------------------------------------------------

For each file or logical group of files, assign a proposed status:

KEEP
ARCHIVE
DELETE CANDIDATE
GENERATED
DUPLICATE / OBSOLETE
NEEDS USER DECISION

IMPORTANT:

"DELETE CANDIDATE" does NOT mean delete it.

It means:

"I think this can probably be removed, but I need the user's approval first."

For anything uncertain, use:

NEEDS USER DECISION

Do not guess.

--------------------------------------------------
4. IDENTIFY DUPLICATES AND CONFLICTS
--------------------------------------------------

Look specifically for:

- duplicate documents
- Vietnamese vs English versions
- old vs new versions
- multiple copies of the same specification
- multiple implementation notes
- old source code replaced by newer source code
- build outputs committed to Git
- generated files
- temporary files
- obsolete binaries
- duplicate scripts
- duplicate configuration
- files whose names suggest they are old but may still be referenced

For each suspected duplicate/obsolete file, explain briefly WHY you think it is redundant.

Do not delete anything.

--------------------------------------------------
5. IDENTIFY IMPORTANT RISKS
--------------------------------------------------

Before proposing removal, check whether the file appears to be referenced by:

- source code
- scripts
- build configuration
- deployment scripts
- tests
- documentation
- README
- Makefiles
- QNX build configuration
- CI/CD
- Git configuration
- other documents

If you cannot determine whether something is safe to remove, mark it:

NEEDS USER DECISION

--------------------------------------------------
6. OUTPUT A REVIEW REPORT
--------------------------------------------------

Do NOT make changes yet.

Give me a concise but comprehensive review containing:

### A. Repository overview

Explain what major types of content currently exist.

### B. Proposed category structure

Show the categories you recommend.

### C. File inventory

Use a table:

| Category | File / Directory | Purpose | Current Status | Proposed Action | Confidence | Reason |
|----------|------------------|---------|----------------|-----------------|------------|--------|

Where Proposed Action is one of:

KEEP
ARCHIVE
DELETE CANDIDATE
GENERATED
DUPLICATE / OBSOLETE
NEEDS USER DECISION

### D. Files recommended for deletion

List them separately.

IMPORTANT:
These are ONLY proposed deletions.
Do not delete them.

For each one give:
- path
- reason
- whether anything references it
- confidence

### E. Files that should probably be archived

List historical material that may still be useful but should not remain mixed with active implementation.

### F. Files that should definitely remain

Identify files that appear important for:
- current implementation
- build
- deployment
- verification
- submission
- project documentation

### G. Ambiguous files

List anything where you cannot confidently determine the correct disposition.

### H. Proposed final repository structure

Show a proposed directory tree, but DO NOT create it yet.

For example:

project/
├── src/
├── include/
├── scripts/
├── tools/
├── tests/
├── docs/
│   ├── specification/
│   ├── implementation/
│   ├── verification/
│   └── reference/
├── archive/
└── ...

This is only a proposal.

==================================================
HARD STOP — WAIT FOR MY APPROVAL
==================================================

After presenting the review, STOP.

Do NOT:
- modify files
- move files
- delete files
- rename files
- create directories
- edit .gitignore
- modify Git
- commit anything

Ask me to review:

1. the proposed categories
2. the files marked DELETE CANDIDATE
3. the files marked ARCHIVE
4. the proposed repository structure
5. any UNCLEAR files

Wait for my explicit approval or corrections.

DO NOT proceed to Phase 2 until I explicitly approve the classification.

==================================================
PHASE 2 — ONLY AFTER USER APPROVAL
==================================================

Once I approve the classification, use my decisions as the source of truth.

Before making changes, summarise exactly what you are about to do.

Then:
- reorganise directories
- move files
- archive historical material
- remove only explicitly approved files
- update references caused by moves
- update documentation paths where necessary
- update .gitignore only where appropriate
- preserve current implementation behaviour
- preserve Git history where possible
- do not delete anything that was not approved

After the cleanup, report:

1. files moved
2. files archived
3. files deleted
4. files renamed
5. references updated
6. .gitignore changes
7. final repository tree
8. any remaining cleanup items

Finally run appropriate validation such as:
- git status
- build / compilation checks where relevant
- tests / verification where relevant
- reference/path checks

Do NOT commit unless I explicitly ask you to commit.

==================================================
MOST IMPORTANT RULE
==================================================

Phase 1 is READ-ONLY.

I want to make the decisions about what stays and what goes.

You are responsible for:
SCAN → UNDERSTAND → CLASSIFY → RECOMMEND → ASK ME

I am responsible for:
APPROVE → MODIFY THE PLAN → AUTHORISE CLEANUP

Never skip the approval gate.