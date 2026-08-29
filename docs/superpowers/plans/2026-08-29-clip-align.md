# Clip filmstrip alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Magnet clip layers into an equal-gap contact sheet whose ruler is the first tear’s hole, with faint grey dotted guides only while dragging.

**Architecture:** Pure `clipAlign()` in `src/clip-align.cpp` derives guides and an optional snap delta from the origin hole plus current clip AABBs. The editor calls it during clip lift-drag and Select-move of a clip, paints guides, and applies the delta to live geometry. No new log op.

**Tech Stack:** Qt6, existing omasnap-smoke, `make check`.

---

### Task 1: Pure solver + smoke

**Files:**
- Create: `src/clip-align.hpp`, `src/clip-align.cpp`
- Create: `tests/clip-align-smoke.hpp`, `tests/clip-align-smoke.cpp`
- Modify: `CMakeLists.txt` (core + smoke sources)
- Modify: `tests/editor-smoke.cpp` (call `runClipAlignSmoke`)

See `docs/superpowers/specs/2026-08-29-clip-align-design.md`.

- [ ] Failing tests for gap/axis, second-slot snap, tallest next-row, Alt, gap 0
- [ ] Implement `clipAlign`
- [ ] `make check` (or at least omasnap-smoke) green
- [ ] Commit

### Task 2: Editor glue

**Files:**
- Modify: `src/editor.hpp`, `src/editor.cpp` (`updateClipLift`, Select move, `paintEdit`)
- Modify: `tests/clip-mapping-smoke.cpp` if a widget drag is cheap
- Modify: `README.md`, `docs/editing-model.md`

- [ ] Call solver on clip drag; Alt ignores; paint dotted grey guides while dragging
- [ ] `make check`
- [ ] Commit
