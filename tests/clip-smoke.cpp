/** @fileoverview Tests the clip engine: copy, punch, native mapping, snap. */
#include "clip-smoke.hpp"

#include "clip.hpp"

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>

bool runClipSmoke(QString &error) {
  QImage source(8, 8, QImage::Format_ARGB32_Premultiplied);
  source.fill(QColor(40, 180, 60, 255));
  source.setPixelColor(2, 3, QColor(220, 50, 0, 255));

  const QRect region(2, 2, 3, 3);
  const QImage tile = copyRect(source, region);
  if (tile.size() != QSize(3, 3) ||
      tile.pixelColor(0, 1) != QColor(220, 50, 0, 255)) {
    error = QStringLiteral("copyRect did not keep the marked pixel");
    return false;
  }

  QImage punched = source;
  punchRect(punched, region);
  if (punched.size() != source.size()) {
    error = QStringLiteral("punchRect must not collapse the image");
    return false;
  }
  if (punched.pixelColor(2, 3).alpha() != 0) {
    error = QStringLiteral("punchRect left pixels in the hole");
    return false;
  }
  if (punched.pixelColor(0, 0) != QColor(40, 180, 60, 255)) {
    error = QStringLiteral("punchRect touched pixels outside the rect");
    return false;
  }

  // Empty / out-of-bounds are no-ops.
  if (!copyRect(source, QRect(20, 20, 2, 2)).isNull()) {
    error = QStringLiteral("copyRect of a miss should be null");
    return false;
  }
  QImage untouched = source;
  punchRect(untouched, QRect());
  if (untouched != source) {
    error = QStringLiteral("empty punchRect changed the image");
    return false;
  }

  const QRect native =
      nativeClipRect(QRectF(2, 2, 3, 3), QSize(8, 8), QSize(8, 8));
  if (native != QRect(2, 2, 3, 3)) {
    error = QStringLiteral("nativeClipRect 1:1 mapping wrong");
    return false;
  }

  // 2× source: logical 2,2 3×3 → native 4,4 6×6.
  const QRect hidpi =
      nativeClipRect(QRectF(2, 2, 3, 3), QSize(8, 8), QSize(16, 16));
  if (hidpi != QRect(4, 4, 6, 6)) {
    error = QStringLiteral("nativeClipRect hidpi mapping wrong");
    return false;
  }

  if (!clipDestSnapped(QRectF(10, 10, 20, 20), QRectF(12, 11, 20, 20), 8.0) ||
      clipDestSnapped(QRectF(10, 10, 20, 20), QRectF(40, 40, 20, 20), 8.0)) {
    error = QStringLiteral("clipDestSnapped threshold wrong");
    return false;
  }
  if (clipSnapThreshold(2.0) != 4.0) {
    error = QStringLiteral("clipSnapThreshold scale mapping wrong");
    return false;
  }

  return true;
}
