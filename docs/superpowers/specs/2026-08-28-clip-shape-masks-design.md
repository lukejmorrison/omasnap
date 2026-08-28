# Clip-out shape masks

Date: 2026-08-28  
Branch: `feat/select-clip-out`  
Status: design locked; awaiting spec review before the implementation plan

## Problem

Clip-out today only locks a **rectangle**. Lifting DHH’s circular X avatar keeps the teal corners and punches a square hole. The job is to clip a **path** — rectangle, ellipse, or freehand lasso — see the hole before committing, then drag the masked pixels out as a layer with a real transparent (or solid) hole left behind.

Click-to-snap is the fast path for that circular avatar: click the object, get an adjustable ellipse, then lift.

## What this is not

- Not a general drawing program, magic-wand editor, or SAM-style subject cutout.
- Not a new linked library or model. Qt6 painter paths, a bounded contour fit, and the existing palette/eyedropper.
- Not a collapse-the-gap tool. That remains Cut (`X`).
- Not an opacity slider. A hole is transparent or a solid colour.

## User-visible behaviour

Stay in **Select** (`V`). Empty-canvas interaction locks a **pixel mask**, not a vector annotation.

1. Pick a clip shape (strip, keys, or cycle).
2. Draw it (or click to snap). A **dotted outline** and **handles** appear. Outside the path is dimmed; inside previews the hole fill (checkerboard if transparent, the swatch if solid).
3. Adjust handles. Change fill from the keyboard. The hole updates live.
4. Grab **inside** the path and **drag to lift**. Release near the hole to snap back (no log entry). Release elsewhere to commit one Clip op: punch/fill the hole, add a `Annotation::Kind::Clip` layer whose bitmap has alpha outside the path.
5. Repeat. `Ctrl+Z` undoes hole and layer together. `Esc` cancels an uncommitted mask.

Shift while drawing an ellipse still forces 1:1 (a circle), matching existing shape tools. Alt still draws from the centre.

### Shape strip

While Select is the current tool, a strip sits **above the toolbar**:

| Chip | First key (stays in Select) | Tooltip |
|---|---|---|
| Rect | `R` | Rectangle · R · V to cycle · press R again to draw |
| Ellipse | `E` | Ellipse · E · V to cycle · press E again to draw |
| Lasso | `F` | Lasso · F · V to cycle · press F again to draw |
| Snap | click, no drag | Snap · click the object · no drag |

`V` (and a second click of the Select toolbar button) **cycles** Rect → Ellipse → Lasso → Snap → Rect. Status pill names the shape (`Clip · ellipse · drag to lock · click to snap · Esc cancels`).

The **bottom-left hotkey legend** gains the clip rows while Select is on, same low-opacity column as today. Hover tooltips on the chips match those rows. Do not add a settings UI.

### Key rule: first press clips, second press draws

`R`, `E`, and `F` currently leave Select and arm the drawing tools. That stays available:

- **First press** while Select is on: only changes the clip-mask shape; stay in Select.
- **Second press of the same key**: leave Select and arm the normal drawing tool (rectangle / ellipse / freehand), the same “press again” pattern as Spotlight (`S`) and Highlighter (`H`).
- Toolbar icons still arm drawing tools in one click, unchanged.

A press-and-release that never exceeds the editor’s existing drag slop is Snap **in every clip shape**, not only when the Snap chip is lit. The Snap chip is the visible “this click will snap” state; cycling to it is optional. A drag that passes slop draws the current Rect / Ellipse / Lasso instead.

### Fill (hole colour)

The existing clip fill fly-out (transparent + palette + custom + eyedropper) stays, labeled with keys. Default fill is transparent.

Fill keys apply **only while a dotted mask is locked** (the path is on screen, not yet lifted). Idle Select does not steal them: `T` still starts Text, `1`–`8` still set annotation colour. Sequence: pick a shape, draw/snap the mask, then `T` / `1`–`8` / `I` / `#`, then grab.

| Key | While a dotted mask is locked | Otherwise |
|---|---|---|
| `T` | Transparent hole | Text tool, unchanged |
| `1`–`8` | Palette fill (already wired on the rect PR) | Annotation colour, unchanged |
| `I` | Eyedropper, sample the screenshot | Eyedropper for annotation colour, unchanged |
| `#` | Type `#RRGGBB` (optional `#RGB`); Enter commits, Esc cancels typing | Ignored |

No `#AARRGGBB` and no alpha slider. `T` is the transparent path. `Esc` (or a snap-back lift) clears the mask; `T` is Text again.

Live preview: the interior of the path shows the fill immediately (checkerboard for transparent). Status names it (`Hole fill transparent · drag inside to clip out` / `Hole fill #E03131 · drag inside to clip out`).

## Engine

Live drag remains editor-only. The log is touched only on release that does not snap back. One `Operation::Type::Clip`. Undo/redo and recents-shelf replay stay exact because the op still reconstructs both hole and layer.

### `ClipOp`

Replace “always a rectangle” with a shape. Native pixels of the composed image at apply-time, same contract as Cut/clip today:

```
enum class ClipShape { Rect, Ellipse, Lasso };

struct ClipOp {
  ClipShape shape = ClipShape::Rect;
  QRect sourceRect;          // bbox; for Rect/Ellipse this is the shape
  QVector<QPoint> points;    // Lasso polygon in native pixels; empty otherwise
  QColor fill;               // invalid or alpha 0 = transparent punch
};
```

Snap is **not** a fourth shape. It produces an Ellipse `ClipOp` (and an ellipse mask the user can still resize) before lift.

JSON (`type: "clip"`):

- `sourceRect` as today `[x,y,w,h]`
- `shape`: `"rect"` | `"ellipse"` | `"lasso"` — omitted means `rect`
- `points`: array of `[x,y]` for lasso only
- `fill`: HexArgb when opaque, omitted when transparent
- `annotation`: dest rect of the lifted layer, as today

No migration shim. Omitted `shape` reads as rect so a working snapshot from the rect-only clip PR still reopens.

### Copy, punch, lift

New helpers in `src/clip.cpp` (keep `copyRect` / `punchRect` / `fillHole` as the rect special case, or thin wrappers):

- `clipPath(const ClipOp &) → QPainterPath` in native space.
- `copyMasked(source, op) → QImage` of `sourceRect` intersected with the image, Format_ARGB32_Premultiplied, pixels **outside** the path alpha 0.
- `fillHole(image, op)` fills the path with `fill`, or punches transparent when `!clipFillOpaque(fill)`.

The lifted `Annotation::Kind::Clip` **image** is that masked tile. `start`/`end` are the dest bbox in annotation space (same as today). Replay copies with `copyMasked` from the composed source **at that op**, then `fillHole` on the composed image, then attaches the tile. Later cuts cannot rewrite an already-torn piece.

Lasso close: if the pointer is near the start point on release, close; otherwise close with a straight segment. Degenerate paths (empty, < 3 points, zero area) are a no-op, no log entry.

Lasso **adjust** after lock is the eight bbox handles (scale/translate the polygon). No vertex editing.

### Click-to-snap

Naive flood-fill from a face click selects cheek, not the circular avatar. Snap must find the **object**, not the local colour.

Algorithm, Qt only, bounded so it stays on the UI thread:

1. Map the click to native pixels.
2. Take a search window centred on the click, cap the window at 512 px on a side, clipped to the composed image.
3. Compute a simple gradient magnitude in that window (Sobel or adjacent-pixel difference on luma).
4. Find the **strongest closed high-contrast contour that encloses the click** (connected edge pixels, or a thresholded gradient ring). Prefer the innermost tight ring that still encloses the click if several exist.
5. Fit an ellipse (or a circle when width/height are within a small ratio) to that contour. Lock an **Ellipse** mask with handles.
6. If no enclosing contour, the window is empty, or the fitted ellipse would cover most of the screenshot, **fail**: status `Nothing to snap · drag a shape instead`. No mask, no log entry.

Do not add OpenCV, ONNX, or a model. Do not run this on a worker unless a later measurement shows a hitch; the 512 px cap exists so it should not.

### Threading

Unchanged: PNG encode, disk, `wl-copy` stay off the UI thread. Mask preview, handle math, and snap stay on the UI thread. Commit still appends one op and `replayLog()` rebuilds.

## Editor integration

- Pixel-clip state today is `pixelClipRect_` (logical QRectF). Generalise to a small editor-only struct: shape, logical rect, optional logical lasso points, fill, lift preview. Not a log entry until release.
- Handle hit-testing for ellipse uses the ellipse bbox handles (same eight as the current rect clip / ellipse annotations). Lasso uses the polygon’s bbox handles.
- Hole-fill fly-out stays next to the locked mask, including lasso/ellipse bboxes.
- `nativeClipRect` stays for mapping a logical rect to native. Lasso points map with the same floor/ceil scaling as cut/clip rects.

## Tests

Headless offscreen, in the existing smoke suite. Failures name the first wrong pixel.

- **Rect** — existing `clip-smoke` / `clip-mapping-smoke` still pass.
- **Ellipse** — synthetic circle (opaque disk on a solid field). Clip the disk with an ellipse mask; corners of the bbox are transparent; hole in the source is transparent (or the chosen fill); lifted layer composite matches the disk.
- **Lasso** — triangle or irregular polygon; outside-path pixels in the tile are alpha 0; hole matches the path.
- **Snap** — fixture of a high-contrast circle (the DHH-avatar case, simplified). Click the centre; fitted ellipse covers the disk and not the field. Click empty field; no op, status set.
- **Undo** — ellipse clip then `Ctrl+Z` restores hole and drops the layer.
- **Fill keys** — with a locked mask, `T` sets transparent; `1` sets palette[0]; `#` + hex + Enter sets custom. After Esc (no mask), `T` arms the text tool. Idle Select without a mask: `T` must not set clip fill.
- **Second-press** — `E` in Select sets ellipse clip shape and `tool_ == Select`; a second `E` arms `Tool::Ellipse`.

`make check` after the behavioural change.

## Docs to update in the same work

- `README.md` — clip-out bullet and the Select / `V` / `R` / `E` / `F` / `T` / `#` rows.
- `docs/editing-model.md` — Clip paragraph: path, alpha tile, snap → ellipse.
- `AGENTS.md` layout line for `src/clip.cpp` if the file overview changes.
- `docs/dependencies.md` — no new rows. If snap stays Qt-only it does not belong there.

## Out of scope

- Neural / SAM / OpenCV subject cutout.
- Vertex-level lasso editing.
- Animated marching ants (a dashed stroke is enough).
- Fill alpha other than 0 or 255.
- Compositor-specific snap (Wayland window outlines). Hyprland-only remains the product; this feature is screenshot pixels, not `hyprctl`.
- Changing Cut, redaction, or pin.

## Files (implementation, not this spec)

| File | Role |
|---|---|
| `src/clip.hpp` / `src/clip.cpp` | Shape, path, copyMasked, fillHole, snap-fit |
| `src/capture.hpp` / `src/capture.cpp` | JSON for shape + points |
| `src/editor.cpp` / `src/editor.hpp` | Strip, keys, legend, preview, lift |
| `tests/clip-smoke.cpp` | Engine pixels |
| `tests/clip-mapping-smoke.cpp` | Editor mapping, snap fixture, undo, keys |
| `README.md`, `docs/editing-model.md` | User-facing + log contract |
