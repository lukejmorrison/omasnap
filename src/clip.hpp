/** @fileoverview Rectangular clip-out engine: copy a native-pixel rect and
 *  punch a transparent hole without collapsing the gap.
 *
 *  Distinct from the Cut tool, which removes a band and shifts the rest.
 *  Coordinates follow Cut: `sourceRect` is native pixels of the composed
 *  image as it existed when the clip was applied; replay applies ops in
 *  order so later cuts see the hole already punched. */
#pragma once

#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>

/** One applied clip. `sourceRect` is inclusive top-left, exclusive of the
 *  pixels past `right()`/`bottom()` in the usual QRect sense (x, y, w, h).
 *  An empty rect, or one that misses the image, is a no-op. */
struct ClipOp {
  QRect sourceRect;
  bool operator==(const ClipOp &) const = default;
};

/** Returns a copy of `sourceRect` intersected with `source`. Null when the
 *  intersection is empty. */
[[nodiscard]] QImage copyRect(const QImage &source, QRect sourceRect);

/** Punches `sourceRect` to transparent. Converts `image` to premultiplied
 *  ARGB when needed so the hole can reveal a backdrop. No-op on an empty
 *  intersection. */
void punchRect(QImage &image, QRect sourceRect);

/** Maps a logical (preview) rectangle onto native source pixels. Floor the
 *  start and ceil the end so a drag covers every pixel it touches, matching
 *  the Cut tool's band mapping. */
[[nodiscard]] QRect nativeClipRect(QRectF logical, QSize preview, QSize source);

/** Annotation-space distance at which a lifted clip snaps back into its
 *  hole. `viewScale` is the editor's annotation-to-widget scale. */
[[nodiscard]] qreal clipSnapThreshold(qreal viewScale);

/** True when `dest` is close enough to `origin` that releasing should restore
 *  the hole instead of committing a new layer. */
[[nodiscard]] bool clipDestSnapped(const QRectF &dest, const QRectF &origin,
                                   qreal threshold);
