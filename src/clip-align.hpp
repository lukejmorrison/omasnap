/** @fileoverview Filmstrip snap for clip layers: hole is the ruler, equal
 *  gap continues the line, next row is tallest-on-row plus that gap.
 *
 *  Derived from current geometry. Nothing is stored in the operation log. */
#pragma once

#include <QLineF>
#include <QRectF>
#include <QVector>
#include <optional>

struct ClipAlignGuide {
  QLineF line;
};

struct ClipAlignResult {
  QVector<ClipAlignGuide> guides;
  std::optional<QPointF> snapDelta;
};

struct ClipAlignInput {
  QRectF hole;
  QVector<QRectF> clips;
  int moving = 0;
  bool altHeld = false;
  qreal enterThreshold = 8.0;
  qreal leaveThreshold = 12.0;
  bool wasSnapped = false;
};

/** Guides and an optional translation to add to the moving clip. Empty when
 *  there is no hole, no clips, or the moving index is out of range. */
[[nodiscard]] ClipAlignResult clipAlign(const ClipAlignInput &input);
