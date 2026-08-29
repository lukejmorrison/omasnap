#include "clip-align-smoke.hpp"

#include "clip-align.hpp"

#include <QString>
#include <cmath>

namespace {

bool near(qreal value, qreal expected) {
  return std::abs(value - expected) < 0.51;
}

} // namespace

bool runClipAlignSmoke(QString &error) {
  const QRectF hole(0, 0, 40, 40);
  const QRectF first(56, 0, 40, 40); // 16 px to the right, same top

  {
    const QRectF drifting(3, 2, 40, 40);
    const ClipAlignResult result =
        clipAlign({hole, {drifting}, 0, false, 8.0, 12.0, false});
    if (!result.snapDelta.has_value() ||
        !near(drifting.left() + result.snapDelta->x(), 0.0) ||
        !near(drifting.top() + result.snapDelta->y(), 0.0)) {
      error = QStringLiteral("origin clip did not snap to the hole");
      return false;
    }
  }

  {
    const QRectF second(110, 2, 40, 40);
    const ClipAlignResult result =
        clipAlign({hole, {first, second}, 1, false, 8.0, 12.0, false});
    if (!result.snapDelta.has_value()) {
      error = QStringLiteral("second clip offered no snap");
      return false;
    }
    const QRectF snapped = second.translated(*result.snapDelta);
    if (!near(snapped.left(), 112.0) || !near(snapped.top(), 0.0)) {
      error = QStringLiteral("second clip did not continue the line at gap 16 "
                             "(left %1 top %2)")
                  .arg(snapped.left())
                  .arg(snapped.top());
      return false;
    }
    if (result.guides.isEmpty()) {
      error = QStringLiteral("second clip snap painted no guides");
      return false;
    }
  }

  {
    const QRectF tall(56, 0, 40, 80);
    const QRectF towardRow(56, 90, 40, 40);
    const ClipAlignResult result =
        clipAlign({hole, {tall, towardRow}, 1, false, 8.0, 12.0, false});
    if (!result.snapDelta.has_value()) {
      error = QStringLiteral("next-row clip offered no snap");
      return false;
    }
    const QRectF snapped = towardRow.translated(*result.snapDelta);
    if (!near(snapped.top(), 96.0)) {
      error = QStringLiteral("next row was not tallest 80 + gap 16 (top %1)")
                  .arg(snapped.top());
      return false;
    }
  }

  {
    const QRectF second(110, 2, 40, 40);
    const ClipAlignResult result =
        clipAlign({hole, {first, second}, 1, true, 8.0, 12.0, false});
    if (result.snapDelta.has_value()) {
      error = QStringLiteral("Alt still snapped");
      return false;
    }
  }

  {
    const QRectF overlapping(10, 10, 40, 40);
    const ClipAlignResult result =
        clipAlign({hole, {overlapping}, 0, false, 8.0, 12.0, false});
    if (result.guides.isEmpty()) {
      error = QStringLiteral("overlapping hole offered no hole guides");
      return false;
    }
  }

  {
    const ClipAlignResult result = clipAlign({});
    if (result.snapDelta.has_value() || !result.guides.isEmpty()) {
      error = QStringLiteral("empty input was not a no-op");
      return false;
    }
  }

  return true;
}
