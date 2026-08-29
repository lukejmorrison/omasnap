# Clip filmstrip alignment

Date: 2026-08-29  
Branch: `feat/clip-align`  
Status: design locked (brainstorm 2026-08-29)

## Problem

Clip-out can tear several pieces out of a screenshot, but a clip layer then moves with a raw pointer delta. There is no way to keep a row or column of clips equally spaced. The job is to magnet clip layers into a **contact sheet** whose ruler is the first tear.

## What this is not

- Not a general Figma alignment engine for arrows, text, or the screenshot’s outer box.
- Not a stored layout grid, Auto Layout, or sibling reflow.
- Not persistent chrome after the pointer is up.
- Not a new log operation or config key.
- Not duplicate-to-next-slot (follow-up PR). `Alt+D` already duplicates any selected layer; plain `D` remains Redact.

## User-visible behaviour

Only **`Annotation::Kind::Clip`** layers magnet. Other kinds are unchanged.

1. Tear clip 1 out of a hole and drop it. The **hole** (the punched path’s AABB in annotation space) is the ruler. The dominant axis of hole→clip 1 is the sheet: more sideways than up/down → a **row**; more vertical → a **column**. The **gap** is the edge-to-edge distance from the hole to clip 1 on that axis.
2. Drag clip 2 (the lift from a new hole, or a later Select-drag of an existing clip). While the pointer is down, faint **grey dotted** guides appear: the current row/column line, the column/row through the slot you are near, and an **estimated next** filmstrip line. If the moving clip is close to a slot, it snaps. **Alt** ignores the magnet. On release the guides vanish; the canvas is just the screenshot and the clips.
3. A **second row** (or second column) sits one **tallest-on-that-row** (or widest-on-that-column) plus the same gap away — a contact sheet, not a square lattice from the hole centre. Different-sized clips: the next line uses the current tallest/widest on that row/column, so a taller later clip pushes the estimated next line out.
4. Siblings do **not** auto-reflow when you move clip 1. The sheet re-derives from current geometry; each clip snaps only while **it** is being dragged.

Idle Select shows no snap lines. Soft threshold (~8 logical px to engage, ~12 to release) so the magnet does not chatter.

### Cheat sheet

The bottom-left legend does **not** currently list `Alt+D` (README does; the legend’s `D / O` row is Redact / OCR). This PR does not add a legend row. A follow-up can add `Alt+D` duplicate and land the copy on the next filmstrip slot.

## Engine

Pure function, UI-thread, Qt only. No new `Operation::Type`. Positions stay on the clip annotations; holes stay on existing `ClipOp`s. Undo is the existing Patch / Clip drop.

```
struct ClipAlignSlot {
  QRectF bounds;          // where the moving clip would sit if snapped
};

struct ClipAlignGuide {
  QLineF line;            // annotation space
};

struct ClipAlignResult {
  QVector<ClipAlignGuide> guides;
  std::optional<QPointF> snapDelta; // add to the moving clip's translation
};

ClipAlignResult clipAlign(const QRectF &hole,           // first clip's hole AABB
                          const QVector<QRectF> &clips, // all clip bounds, moving last
                          int movingIndex,
                          bool altHeld);
```

Rules:

- **Origin.** Oldest remaining clip layer (lowest index among `Kind::Clip`) owns the sheet. Its matching `ClipOp.sourceRect` mapped into annotation space is `hole`. If that clip is deleted, the next remaining clip becomes origin. No clips → no guides.
- **Axis.** Vector from hole centre to origin-clip centre. `|dx| >= |dy|` → horizontal sheet (rows); else vertical (columns).
- **Gap.** Edge-to-edge on that axis, hole AABB vs origin clip AABB. If the origin clip overlaps the hole on that axis, gap is 0 and the magnet still offers centre/edge alignment to the hole but not a filmstrip pitch of 0 (treat gap `< 1` as “no pitch yet” — only hole-edge/centre guides).
- **Cross-axis.** Snap the moving clip’s **centre** to the row/column line (a tall clip straddles the line, matching the sketch).
- **Along-axis.** Snap so the edge-to-edge gap to the previous clip (or to the hole, for the first slot after the hole) equals `gap`.
- **Filmstrip pitch.** For a horizontal sheet, each row’s height is the max clip height currently in that row. Next row **top** = this row’s top + that height + gap. The estimated next-row guide is shown while dragging. Symmetric for a vertical sheet using widths.
- **Targets while dragging.** Hole edges and centre; existing clip edges and centres on the sheet; the next empty slot along the line; the next empty filmstrip row/column. Independent X and Y (can snap to a column and a row at once — the empty cross).
- **Alt.** `altHeld` → `guides` may still paint (so you can see what you are ignoring) but `snapDelta` is empty.
- **Threshold.** Engage when the unsnapped bounds are within 8 px of a slot on that axis; hold until 12 px away.

Editor glue:

- Call `clipAlign` from clip lift-drag and from Select move of a clip layer (`mouseMoveEvent`).
- Paint `guides` in `paintEdit` only while that drag is active, grey dotted (`QColor(186,192,202, ~90)`, `Qt::DotLine`).
- Apply `snapDelta` to the live annotation (lift preview or `translateAnnotation`), not to the log, until release.
- Resize handles of a clip do not run the filmstrip magnet (move only).

Hole mapping: `sourceRect` is native pixels of the composed image at the Clip op. Convert to annotation space with the same scale already used for clip tiles (`nativeClipRect` inverted).

## Tests

Headless, in the smoke suite. New `tests/clip-align-smoke.cpp` for the pure function (no widget). A short mapping case in `clip-mapping-smoke` that drags a second clip near the next slot and checks it landed on the gap, and that Alt does not snap.

- Origin to the right of a 40×40 hole by 16 px → axis horizontal, gap 16.
- Second clip near the next slot snaps so its left = first.right + 16, centres share Y.
- Tall 40×80 clip on that row → next row top = rowTop + 80 + 16.
- `altHeld` → no `snapDelta`.
- Overlapping hole (gap 0) → no filmstrip pitch; hole centre/edges still offered.
- No clip layers → empty result.

`make check` after the behavioural change.

## Docs to update in the same work

- `README.md` — clip-out / Select: while dragging a clip, faint dotted guides snap it to the hole and to equal-gap filmstrip slots; Alt ignores.
- `docs/editing-model.md` — one sentence: clip-layer drag may magnet to other clips; the log still only stores positions.

## Out of scope

- Snap to screenshot AABB, arrows, text, or non-clip layers.
- Persistent guides after mouse up.
- Stored ruler / new Operation type.
- Auto-reflow of siblings.
- Duplicate-to-next-slot and adding `Alt+D` to the hotkey legend (follow-up).
- Config keys, settings UI.

## Files

| File | Role |
|---|---|
| `src/clip-align.hpp` / `src/clip-align.cpp` | Pure snap solver |
| `src/editor.cpp` / `src/editor.hpp` | Drag hook, paint guides |
| `tests/clip-align-smoke.cpp` | Engine |
| `tests/clip-mapping-smoke.cpp` | Editor mapping + Alt |
| `CMakeLists.txt` | Smoke sources |
| `README.md`, `docs/editing-model.md` | User-facing + log contract |
