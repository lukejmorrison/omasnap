/** @fileoverview Tests the clip engine: copy, punch, native mapping, snap. */
#include "clip-smoke.hpp"

#include "clip.hpp"
#include "capture.hpp"

#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QFile>
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

  QImage filled = source;
  fillHole(filled, region, QColor(10, 132, 255, 255));
  if (filled.pixelColor(2, 3) != QColor(10, 132, 255, 255)) {
    error = QStringLiteral("fillHole did not paint the solid infill");
    return false;
  }
  if (filled.pixelColor(0, 0) != QColor(40, 180, 60, 255)) {
    error = QStringLiteral("fillHole touched pixels outside the rect");
    return false;
  }
  QImage viaTransparent = source;
  fillHole(viaTransparent, region, QColor(0, 0, 0, 0));
  if (viaTransparent.pixelColor(2, 3).alpha() != 0) {
    error = QStringLiteral("fillHole with alpha 0 did not punch");
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
  if (clipSnapEnterThreshold(2.0) != 7.0 ||
      clipSnapLeaveThreshold(2.0) != 10.0) {
    error = QStringLiteral("clip snap hysteresis thresholds wrong");
    return false;
  }

  QImage composed(8, 8, QImage::Format_ARGB32_Premultiplied);
  composed.fill(QColor(220, 50, 0, 255));
  QImage existing(3, 3, QImage::Format_ARGB32_Premultiplied);
  existing.fill(QColor(10, 132, 255, 255));
  const QImage kept =
      resolveClipTile(composed, QRect(2, 2, 3, 3), existing, false);
  if (kept.size() != existing.size() ||
      kept.pixelColor(1, 1) != QColor(10, 132, 255, 255)) {
    error = QStringLiteral("resolveClipTile recopied when the tile was present");
    return false;
  }
  const QImage recopied =
      resolveClipTile(composed, QRect(2, 2, 3, 3), existing, true);
  if (recopied.pixelColor(0, 0) != QColor(220, 50, 0, 255)) {
    error = QStringLiteral("resolveClipTile skipped copy when prefix changed");
    return false;
  }
  const QImage missing =
      resolveClipTile(composed, QRect(2, 2, 3, 3), QImage(), false);
  if (missing.pixelColor(0, 0) != QColor(220, 50, 0, 255)) {
    error = QStringLiteral("resolveClipTile skipped copy when the tile was null");
    return false;
  }

  QImage pngTile(2, 2, QImage::Format_ARGB32);
  pngTile.fill(QColor(9, 8, 7, 255));
  QByteArray pngBytes;
  QBuffer pngBuffer(&pngBytes);
  pngBuffer.open(QIODevice::WriteOnly);
  pngTile.save(&pngBuffer, "PNG");
  const QString b64 = QString::fromLatin1(pngBytes.toBase64());
  const QString jsonPath =
      QDir::temp().filePath(QStringLiteral("omasnap-clip-png-gate.json"));
  const auto writeLog = [&](const QString &tool) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;
    const QString json = QStringLiteral(
        "{\"version\":1,\"index\":1,\"nextId\":\"2\",\"nextMarker\":1,\"ops\":["
        "{\"type\":\"annotate\",\"annotation\":{\"id\":\"1\",\"tool\":\"%1\","
        "\"start\":[0,0],\"end\":[4,4],\"color\":\"#ffff0000\",\"size\":4,"
        "\"png\":\"%2\"}}]}")
                             .arg(tool, b64);
    return file.write(json.toUtf8()) > 0;
  };
  if (!writeLog(QStringLiteral("rectangle"))) {
    error = QStringLiteral("could not write rectangle png-gate log");
    return false;
  }
  OperationLog rectangleLog;
  QString loadError;
  if (!loadOperationLog(jsonPath, rectangleLog, loadError) ||
      rectangleLog.ops.isEmpty() ||
      rectangleLog.ops.constFirst().annotations.isEmpty()) {
    error = QStringLiteral("rectangle png-gate log failed to load: %1")
                .arg(loadError);
    return false;
  }
  if (!rectangleLog.ops.constFirst().annotations.constFirst().image.isNull()) {
    error = QStringLiteral("annotationFromJson loaded png onto a non-Clip kind");
    return false;
  }
  if (!writeLog(QStringLiteral("clip"))) {
    error = QStringLiteral("could not write clip png-gate log");
    return false;
  }
  OperationLog clipLog;
  if (!loadOperationLog(jsonPath, clipLog, loadError) ||
      clipLog.ops.isEmpty() || clipLog.ops.constFirst().annotations.isEmpty() ||
      clipLog.ops.constFirst().annotations.constFirst().image.isNull() ||
      clipLog.ops.constFirst().annotations.constFirst().image.pixelColor(0, 0) !=
          QColor(9, 8, 7, 255)) {
    error = QStringLiteral("annotationFromJson skipped png on a Clip kind");
    return false;
  }
  QFile::remove(jsonPath);

  return true;
}
