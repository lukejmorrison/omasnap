#include "clip-align.hpp"

#include <Qt>
#include <algorithm>
#include <cmath>

namespace {

qreal pick(qreal current, const QVector<qreal> &candidates, qreal threshold) {
  qreal best = current;
  qreal bestDist = threshold + 1.0;
  for (const qreal candidate : candidates) {
    const qreal distance = std::abs(candidate - current);
    if (distance <= threshold && distance < bestDist) {
      bestDist = distance;
      best = candidate;
    }
  }
  return best;
}

void addUnique(QVector<qreal> &values, qreal value) {
  for (const qreal existing : values) {
    if (std::abs(existing - value) < 0.01)
      return;
  }
  values.push_back(value);
}

QLineF spanH(qreal y, const QRectF &hole, const QVector<QRectF> &clips) {
  qreal lo = hole.left();
  qreal hi = hole.right();
  for (const QRectF &clip : clips) {
    lo = std::min(lo, clip.left());
    hi = std::max(hi, clip.right());
  }
  return {lo - 12.0, y, hi + 12.0, y};
}

QLineF spanV(qreal x, const QRectF &hole, const QVector<QRectF> &clips) {
  qreal lo = hole.top();
  qreal hi = hole.bottom();
  for (const QRectF &clip : clips) {
    lo = std::min(lo, clip.top());
    hi = std::max(hi, clip.bottom());
  }
  return {x, lo - 12.0, x, hi + 12.0};
}

} // namespace

ClipAlignResult clipAlign(const ClipAlignInput &input) {
  ClipAlignResult result;
  if (input.hole.isEmpty() || input.clips.isEmpty() || input.moving < 0 ||
      input.moving >= input.clips.size())
    return result;

  const QRectF moving = input.clips.at(input.moving);
  if (moving.isEmpty())
    return result;

  const QRectF origin = input.clips.constFirst();
  const QPointF holeCenter = input.hole.center();
  const QPointF originCenter = origin.center();
  const qreal dx = originCenter.x() - holeCenter.x();
  const qreal dy = originCenter.y() - holeCenter.y();
  const bool horizontal = std::abs(dx) >= std::abs(dy);

  qreal gap = 0.0;
  if (horizontal) {
    if (origin.left() >= input.hole.right())
      gap = origin.left() - input.hole.right();
    else if (origin.right() <= input.hole.left())
      gap = input.hole.left() - origin.right();
  } else {
    if (origin.top() >= input.hole.bottom())
      gap = origin.top() - input.hole.bottom();
    else if (origin.bottom() <= input.hole.top())
      gap = input.hole.top() - origin.bottom();
  }

  QVector<qreal> xs;
  QVector<qreal> ys;
  addUnique(xs, input.hole.left());
  addUnique(xs, input.hole.center().x() - moving.width() / 2.0);
  addUnique(xs, input.hole.right() - moving.width());
  addUnique(ys, input.hole.top());
  addUnique(ys, input.hole.center().y() - moving.height() / 2.0);
  addUnique(ys, input.hole.bottom() - moving.height());

  const bool pitched = gap >= 1.0;
  if (pitched && horizontal) {
    const qreal rowCenter = originCenter.y();
    addUnique(ys, rowCenter - moving.height() / 2.0);
    addUnique(xs, input.hole.right() + gap);
    addUnique(xs, input.hole.left() - gap - moving.width());

    qreal rowTop = origin.top();
    qreal tallest = origin.height();
    for (int index = 0; index < input.clips.size(); ++index) {
      if (index == input.moving)
        continue;
      const QRectF &clip = input.clips.at(index);
      if (std::abs(clip.center().y() - rowCenter) >
          std::max(clip.height(), origin.height()) / 2.0 + gap)
        continue;
      rowTop = std::min(rowTop, clip.top());
      tallest = std::max(tallest, clip.height());
      addUnique(xs, clip.right() + gap);
      addUnique(xs, clip.left() - gap - moving.width());
      addUnique(xs, clip.left());
    }
    addUnique(xs, origin.right() + gap);
    addUnique(xs, origin.left() - gap - moving.width());
    addUnique(ys, rowTop + tallest + gap);
    addUnique(ys, rowTop - gap - moving.height());
  } else if (pitched) {
    const qreal colCenter = originCenter.x();
    addUnique(xs, colCenter - moving.width() / 2.0);
    addUnique(ys, input.hole.bottom() + gap);
    addUnique(ys, input.hole.top() - gap - moving.height());

    qreal colLeft = origin.left();
    qreal widest = origin.width();
    for (int index = 0; index < input.clips.size(); ++index) {
      if (index == input.moving)
        continue;
      const QRectF &clip = input.clips.at(index);
      if (std::abs(clip.center().x() - colCenter) >
          std::max(clip.width(), origin.width()) / 2.0 + gap)
        continue;
      colLeft = std::min(colLeft, clip.left());
      widest = std::max(widest, clip.width());
      addUnique(ys, clip.bottom() + gap);
      addUnique(ys, clip.top() - gap - moving.height());
      addUnique(ys, clip.top());
    }
    addUnique(ys, origin.bottom() + gap);
    addUnique(ys, origin.top() - gap - moving.height());
    addUnique(xs, colLeft + widest + gap);
    addUnique(xs, colLeft - gap - moving.width());
  }

  const qreal threshold =
      input.wasSnapped ? input.leaveThreshold : input.enterThreshold;
  const qreal snappedLeft = pick(moving.left(), xs, threshold);
  const qreal snappedTop = pick(moving.top(), ys, threshold);
  const QPointF delta(snappedLeft - moving.left(), snappedTop - moving.top());
  if (!input.altHeld && (std::abs(delta.x()) > 0.001 || std::abs(delta.y()) > 0.001))
    result.snapDelta = delta;

  const QRectF shown = moving.translated(result.snapDelta.value_or(QPointF()));
  if (pitched && horizontal) {
    result.guides.push_back({spanH(originCenter.y(), input.hole, input.clips)});
    result.guides.push_back(
        {spanH(shown.center().y(), input.hole, input.clips)});
    result.guides.push_back({spanV(shown.center().x(), input.hole, input.clips)});
  } else if (pitched) {
    result.guides.push_back({spanV(originCenter.x(), input.hole, input.clips)});
    result.guides.push_back(
        {spanV(shown.center().x(), input.hole, input.clips)});
    result.guides.push_back({spanH(shown.center().y(), input.hole, input.clips)});
  } else {
    result.guides.push_back({spanH(holeCenter.y(), input.hole, input.clips)});
    result.guides.push_back({spanV(holeCenter.x(), input.hole, input.clips)});
  }
  return result;
}
