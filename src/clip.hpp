/** @fileoverview Rectangular clip-out engine: copy a native-pixel rect and
 *  punch a transparent hole without collapsing the gap.
 *
 *  Distinct from the Cut tool, which removes a band and shifts the rest.
 *  Coordinates follow Cut: `sourceRect` is native pixels of the composed
 *  image as it existed when the clip was applied; replay applies ops in
 *  order so later cuts see the hole already punched. */
#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>

/** One applied clip. `sourceRect` is inclusive top-left, exclusive of the
 *  pixels past `right()`/`bottom()` in the usual QRect sense (x, y, w, h).
 *  An empty rect, or one that misses the image, is a no-op. `fill` is the
 *  hole infill: default (invalid or alpha 0) punches transparent so a
 *  backdrop shows through; a solid color paints that swatch into the hole. */
struct ClipOp {
  QRect sourceRect;
  QColor fill;
  bool operator==(const ClipOp &) const = default;
};

[[nodiscard]] inline bool clipFillOpaque(const QColor &fill) {
  return fill.isValid() && fill.alpha() > 0;
}

/** Returns a copy of `sourceRect` intersected with `source`. Null when the
 *  intersection is empty. */
[[nodiscard]] QImage copyRect(const QImage &source, QRect sourceRect);

/** Punches `sourceRect` to transparent. Converts `image` to premultiplied
 *  ARGB when needed so the hole can reveal a backdrop. No-op on an empty
 *  intersection. */
void punchRect(QImage &image, QRect sourceRect);

/** Fills `sourceRect` with `fill`. Transparent / invalid `fill` punches a
 *  hole, same as `punchRect`. */
void fillHole(QImage &image, QRect sourceRect, const QColor &fill);

/** Maps a logical (preview) rectangle onto native source pixels. Floor the
 *  start and ceil the end so a drag covers every pixel it touches, matching
 *  the Cut tool's band mapping. */
[[nodiscard]] QRect nativeClipRect(QRectF logical, QSize preview, QSize source);

/** Annotation-space distance at which a lifted clip *enters* the snap zone
 *  (~14 widget px). `viewScale` is the editor's annotation-to-widget scale.
 *  Dest is never clamped to the hole while dragging; snap applies on release. */
[[nodiscard]] qreal clipSnapEnterThreshold(qreal viewScale);

/** Annotation-space distance at which a lifted clip *leaves* the snap zone
 *  (~20 widget px). Wider than enter so the snap ghost does not chatter. */
[[nodiscard]] qreal clipSnapLeaveThreshold(qreal viewScale);

[[nodiscard]] inline qreal clipSnapThreshold(qreal viewScale) {
  return clipSnapEnterThreshold(viewScale);
}

/** True when `dest` is close enough to `origin` that releasing should restore
 *  the hole instead of committing a new layer. */
[[nodiscard]] bool clipDestSnapped(const QRectF &dest, const QRectF &origin,
                                   qreal threshold);

/** Tile for a Clip op. Keep `existing` (from beginClipLift / JSON png / a
 *  prior commit) unless it is null or the cut/clip prefix before that op
 *  changed. */
[[nodiscard]] QImage resolveClipTile(const QImage &composed, QRect sourceRect,
                                     const QImage &existing, bool prefixChanged);
