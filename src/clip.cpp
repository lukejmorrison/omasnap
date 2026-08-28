/** @fileoverview Rectangular clip-out engine: copy a region and punch a hole. */
#include "clip.hpp"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <Qt>
#include <algorithm>
#include <cmath>

QImage copyRect(const QImage &source, QRect sourceRect) {
  if (source.isNull())
    return {};
  sourceRect = sourceRect.intersected(source.rect());
  if (sourceRect.isEmpty())
    return {};
  return source.copy(sourceRect);
}

void punchRect(QImage &image, QRect sourceRect) {
  if (image.isNull())
    return;
  sourceRect = sourceRect.intersected(image.rect());
  if (sourceRect.isEmpty())
    return;
  if (image.format() != QImage::Format_ARGB32 &&
      image.format() != QImage::Format_ARGB32_Premultiplied)
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&image);
  painter.setCompositionMode(QPainter::CompositionMode_Clear);
  painter.fillRect(sourceRect, Qt::transparent);
}

void fillHole(QImage &image, QRect sourceRect, const QColor &fill) {
  if (!clipFillOpaque(fill)) {
    punchRect(image, sourceRect);
    return;
  }
  if (image.isNull())
    return;
  sourceRect = sourceRect.intersected(image.rect());
  if (sourceRect.isEmpty())
    return;
  if (image.format() != QImage::Format_ARGB32 &&
      image.format() != QImage::Format_ARGB32_Premultiplied)
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&image);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(sourceRect, fill);
}

QRect nativeClipRect(QRectF logical, QSize preview, QSize source) {
  logical = logical.normalized();
  if (logical.isEmpty() || !preview.isValid() || preview.width() <= 0 ||
      preview.height() <= 0 || !source.isValid() || source.width() <= 0 ||
      source.height() <= 0)
    return {};
  const qreal scaleX =
      source.width() / static_cast<qreal>(preview.width());
  const qreal scaleY =
      source.height() / static_cast<qreal>(preview.height());
  const int left = static_cast<int>(std::floor(logical.left() * scaleX));
  const int top = static_cast<int>(std::floor(logical.top() * scaleY));
  const int right = static_cast<int>(std::ceil(logical.right() * scaleX));
  const int bottom = static_cast<int>(std::ceil(logical.bottom() * scaleY));
  const QRect native(QPoint(left, top), QPoint(right - 1, bottom - 1));
  return native.intersected(QRect(QPoint(), source));
}

qreal clipSnapEnterThreshold(qreal viewScale) {
  return 14.0 / std::max<qreal>(0.001, viewScale);
}

qreal clipSnapLeaveThreshold(qreal viewScale) {
  return 20.0 / std::max<qreal>(0.001, viewScale);
}

bool clipDestSnapped(const QRectF &dest, const QRectF &origin,
                     qreal threshold) {
  const QPointF delta = dest.center() - origin.center();
  return std::hypot(delta.x(), delta.y()) <= threshold;
}

QImage resolveClipTile(const QImage &composed, QRect sourceRect,
                       const QImage &existing, bool prefixChanged) {
  if (!existing.isNull() && !prefixChanged)
    return existing;
  return copyRect(composed, sourceRect);
}
