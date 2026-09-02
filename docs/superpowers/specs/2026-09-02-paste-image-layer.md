# Paste/import an image as a layer

Date: 2026-09-02
Branch: `feat/paste-image-layer` (to cut)
Status: workstream opened, not implemented
GitHub: https://github.com/lukejmorrison/omasnap/issues/1

## Problem

OmaSnap can open an existing file (`--file`, Files → **Edit in OmaSnap**) and can lift pixels *from this capture* (Clip). It cannot drop a *second* image — clipboard or file — onto the capture as a layer. The workaround is a new capture or an external editor.

## What this is not

- Not a drawing app, layer panel, blend-mode stack, or multi-file importer.
- Not `--clipboard` (that still *replaces* the editor contents with the clipboard image).
- Not Clip. Clip punches a hole in this capture. Paste must not.

## User-visible behaviour

1. Toolbar tools cluster gets a clipboard/paste icon (vector, `src/icons.cpp`).
2. Click pastes the Wayland clipboard image as a selected, movable, resizable bitmap layer.
3. Empty / non-image clipboard → status pill, no op.
4. Later: the same control expands to a fly-out with **Import file…** (portal, one image).
5. `V` stays Select / clip-shape. Do not steal it. `Ctrl+Shift+V` is optional.

## Engine

- New `Annotation::Kind` (e.g. `Image`) and a log `Operation`. Undo/redo/recents/JSON sidecar must round-trip the bitmap the same way clip tiles do.
- Decode off the UI thread. Commit on the UI thread as one op.
- Render stays a pure function of source + log. Do not bake into `capture_.source`.

## Split

- #3 Toolbar clipboard paste/import icon
- #2 Paste clipboard image as an undoable bitmap layer
- #4 Expand paste control into a file-import fly-out (after MVP)
