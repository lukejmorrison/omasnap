/** @fileoverview Clip-out engine: copy a path and punch a hole. */
#include "clip.hpp"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <optional>

namespace {

void ensurePremultiplied(QImage &image) {
  if (image.format() != QImage::Format_ARGB32 &&
      image.format() != QImage::Format_ARGB32_Premultiplied)
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QRectF nativeScale(QSize preview, QSize source) {
  if (!preview.isValid() || preview.width() <= 0 || preview.height() <= 0 ||
      !source.isValid() || source.width() <= 0 || source.height() <= 0)
    return {};
  return QRectF(0, 0, source.width() / static_cast<qreal>(preview.width()),
                source.height() / static_cast<qreal>(preview.height()));
}

int lumaAt(const QImage &image, int x, int y) {
  return qGray(image.pixel(x, y));
}

} // namespace

QPainterPath clipPath(const ClipOp &clip) {
  QPainterPath path;
  const QRect box = clip.sourceRect;
  if (box.isEmpty())
    return path;
  switch (clip.shape) {
  case ClipShape::Ellipse:
    path.addEllipse(QRectF(box));
    break;
  case ClipShape::Lasso: {
    if (clip.points.size() < 3)
      return {};
    path.moveTo(clip.points.constFirst());
    for (int i = 1; i < clip.points.size(); ++i)
      path.lineTo(clip.points.at(i));
    path.closeSubpath();
    break;
  }
  case ClipShape::Rect:
    path.addRect(QRectF(box));
    break;
  }
  return path;
}

QImage copyRect(const QImage &source, QRect sourceRect) {
  if (source.isNull())
    return {};
  sourceRect = sourceRect.intersected(source.rect());
  if (sourceRect.isEmpty())
    return {};
  return source.copy(sourceRect);
}

QImage copyMasked(const QImage &source, const ClipOp &clip) {
  QImage tile = copyRect(source, clip.sourceRect);
  if (tile.isNull())
    return {};
  if (clip.shape == ClipShape::Rect)
    return tile;
  ensurePremultiplied(tile);
  QImage mask(tile.size(), QImage::Format_ARGB32_Premultiplied);
  mask.fill(Qt::transparent);
  QPainter maskPainter(&mask);
  maskPainter.setRenderHint(QPainter::Antialiasing, true);
  maskPainter.setPen(Qt::NoPen);
  maskPainter.setBrush(Qt::white);
  QPainterPath path = clipPath(clip);
  path.translate(-clip.sourceRect.topLeft());
  maskPainter.drawPath(path);
  maskPainter.end();
  QPainter painter(&tile);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, mask);
  return tile;
}

void punchRect(QImage &image, QRect sourceRect) {
  if (image.isNull())
    return;
  sourceRect = sourceRect.intersected(image.rect());
  if (sourceRect.isEmpty())
    return;
  ensurePremultiplied(image);
  QPainter painter(&image);
  painter.setCompositionMode(QPainter::CompositionMode_Clear);
  painter.fillRect(sourceRect, Qt::transparent);
}

void fillHole(QImage &image, QRect sourceRect, const QColor &fill) {
  fillHole(image, ClipOp{ClipShape::Rect, sourceRect, {}, fill});
}

void fillHole(QImage &image, const ClipOp &clip) {
  if (image.isNull())
    return;
  const QRect box = clip.sourceRect.intersected(image.rect());
  if (box.isEmpty())
    return;
  const QPainterPath path = clipPath(clip);
  if (path.isEmpty())
    return;
  ensurePremultiplied(image);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  if (!clipFillOpaque(clip.fill)) {
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillPath(path, Qt::transparent);
    return;
  }
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillPath(path, clip.fill);
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

QPointF nativeClipPoint(QPointF logical, QSize preview, QSize source) {
  const QRectF scale = nativeScale(preview, source);
  if (scale.isEmpty())
    return {};
  return QPointF(logical.x() * scale.width(), logical.y() * scale.height());
}

ClipOp nativeClipOp(ClipShape shape, QRectF logical,
                    const QVector<QPointF> &logicalPoints, QSize preview,
                    QSize source, const QColor &fill) {
  ClipOp clip;
  clip.shape = shape;
  clip.fill = fill;
  if (shape == ClipShape::Lasso) {
    clip.points.reserve(logicalPoints.size());
    QRectF bounds;
    for (const QPointF &point : logicalPoints) {
      const QPointF native = nativeClipPoint(point, preview, source);
      clip.points.push_back(native);
      if (bounds.isNull())
        bounds = QRectF(native, QSizeF(0.01, 0.01));
      else
        bounds |= QRectF(native, QSizeF(0.01, 0.01));
    }
    if (!bounds.isEmpty()) {
      const int left = static_cast<int>(std::floor(bounds.left()));
      const int top = static_cast<int>(std::floor(bounds.top()));
      const int right = static_cast<int>(std::ceil(bounds.right()));
      const int bottom = static_cast<int>(std::ceil(bounds.bottom()));
      clip.sourceRect =
          QRect(QPoint(left, top), QPoint(right - 1, bottom - 1))
              .intersected(QRect(QPoint(), source));
    }
    return clip;
  }
  clip.sourceRect = nativeClipRect(logical, preview, source);
  return clip;
}

std::optional<QRect> snapEllipseRect(const QImage &source, QPoint click) {
  if (source.isNull() || !source.rect().contains(click))
    return std::nullopt;
  const QImage img = source.convertToFormat(QImage::Format_ARGB32);
  constexpr int kRays = 36;
  constexpr int kMax = 256;
  constexpr int kThresh = 28;
  constexpr int kMinHits = 12;
  QVector<QPoint> hits;
  hits.reserve(kRays);
  for (int i = 0; i < kRays; ++i) {
    const qreal ang = static_cast<qreal>(i) * (2.0 * M_PI / kRays);
    const qreal dx = std::cos(ang);
    const qreal dy = std::sin(ang);
    int prev = lumaAt(img, click.x(), click.y());
    int bestS = -1;
    int bestD = 0;
    for (int s = 4; s <= kMax; ++s) {
      const int x = click.x() + static_cast<int>(std::lround(dx * s));
      const int y = click.y() + static_cast<int>(std::lround(dy * s));
      if (!img.rect().contains(x, y))
        break;
      const int L = lumaAt(img, x, y);
      const int d = std::abs(L - prev);
      if (d > bestD) {
        bestD = d;
        bestS = s;
      }
      prev = L;
    }
    if (bestD > kThresh && bestS > 0) {
      hits.push_back(
          QPoint(click.x() + static_cast<int>(std::lround(dx * bestS)),
                 click.y() + static_cast<int>(std::lround(dy * bestS))));
    }
  }
  if (hits.size() < kMinHits)
    return std::nullopt;
  int minX = hits.constFirst().x();
  int maxX = minX;
  int minY = hits.constFirst().y();
  int maxY = minY;
  for (const QPoint &hit : hits) {
    minX = std::min(minX, hit.x());
    maxX = std::max(maxX, hit.x());
    minY = std::min(minY, hit.y());
    maxY = std::max(maxY, hit.y());
  }
  QRect box(QPoint(minX, minY), QPoint(maxX, maxY));
  box = box.intersected(source.rect());
  if (box.width() < 6 || box.height() < 6)
    return std::nullopt;
  const qreal cover = static_cast<qreal>(box.width()) * box.height() /
                      std::max(1, source.width() * source.height());
  if (cover > 0.70)
    return std::nullopt;
  const QRectF ellipse(box);
  if (!ellipse.contains(click))
    return std::nullopt;
  const qreal ratio = box.width() / std::max(1.0, static_cast<qreal>(box.height()));
  if (ratio > 1.0 / 1.12 && ratio < 1.12) {
    const int side = std::max(box.width(), box.height());
    QRect circle(0, 0, side, side);
    circle.moveCenter(box.center());
    box = circle.intersected(source.rect());
  }
  return box;
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
  return resolveClipTile(composed,
                         ClipOp{ClipShape::Rect, sourceRect, {}, {}}, existing,
                         prefixChanged);
}

QImage resolveClipTile(const QImage &composed, const ClipOp &clip,
                       const QImage &existing, bool prefixChanged) {
  if (!existing.isNull() && !prefixChanged)
    return existing;
  return copyMasked(composed, clip);
}
