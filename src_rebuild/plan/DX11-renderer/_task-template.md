# Task template — DX11 renderer rewrite

Every task in `TASKS.md` gets its own task file `src_rebuild/plan/DX11-renderer/<idx>-<title>.md`
**before it is implemented**. Copy this file, fill in the frontmatter and
sections, and mark progress in the checkbox lists as you work.

## Frontmatter rules

- `id` — the task id from `TASKS.md` (e.g. `T0.1`).
- `phase` — which Phase it belongs to: `Phase 0 — Architecture & spike`,
  `Phase 1 — Core DX11 renderer`, `Phase 2 — Stereo natively`,
  `Phase 3 — Color modes & split-screen`, `Phase 4 — Integration & cleanup`.
- `milestone` — one of `M0`..`M4` (each milestone = one phase).
- `status` — `planned` | `in_progress` | `blocked` | `done`.

## Body sections

- **Goal** — what this task delivers, in one or two sentences.
- **Context** — current state, the code it touches, and why it matters.
- **Steps** — concrete, ordered, checkable steps.
- **Acceptance criteria** — how to verify the task is truly done.
- **References** — file paths / functions / docs this task builds on.

---

# <id> — <title>

|            | |
|------------|-|
| **Phase**  | Phase N — … |
| **Milestone** | M_N — … |
| **Status** | planned |

## Goal

…

## Context

…

## Steps

- [ ] …

## Acceptance criteria

- [ ] …

## References

- …