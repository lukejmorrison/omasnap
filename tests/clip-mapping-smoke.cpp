/** @fileoverview Drives pixel-clip: marquee, lift, hole, backdrop, undo.
 *
 *  A banded fixture makes the torn-off tile and the hole checkable by colour.
 *  Direct applyClipForTest covers the operation log; a widget drag covers the
 *  empty-marquee → lift path. Failures name the first wrong pixel.
 */
#include "clip-mapping-smoke.hpp"

#include "capture.hpp"
#include "editor.hpp"
#include "image-fixture.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTest>

namespace {

CaptureData fixtureCapture(const QImage &source) {
  CaptureData capture;
  capture.monitor.name = QStringLiteral("TEST");
  capture.monitor.geometry = {0, 0, 800, 600};
  capture.monitor.pixelSize = {800, 600};
  capture.monitor.scale = 1.0;
  capture.source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  capture.previewSize = source.size();
  return capture;
}

QPoint screenOf(const CaptureEditor &editor, qreal ax, qreal ay) {
  return editor.toScreenPointForTest(QPointF(ax, ay)).toPoint();
}

bool saveGrab(CaptureEditor &editor, const QString &path, QString &error) {
  const QImage grab = editor.grab().toImage();
  if (grab.save(path, "PNG"))
    return true;
  error = QStringLiteral("could not write %1").arg(path);
  return false;
}

} // namespace

bool runClipMappingSmoke(QApplication &application, const QString &outputRoot,
                         QString &error) {
  constexpr int kWidth = 160;
  constexpr int kBand = 16;
  constexpr int kBands = 10;
  const QImage source = rowBandImage(kWidth, kBand, kBands);
  QDir().mkpath(QFileInfo(outputRoot).path());

  // Direct apply: punch the green band (index 3, y 48..64) and park it to
  // the right of the image so the canvas grows and the hole shows Slate.
  {
    CaptureEditor editor(fixtureCapture(source),
                         CaptureEditor::CaptureMode::File);
    editor.resize(800, 600);
    editor.show();
    application.processEvents();
    editor.applyClipForTest(QRectF(0, 48, kWidth, kBand),
                            QRectF(kWidth + 20, 48, kWidth, kBand));
    application.processEvents();

    const QImage composed = editor.composedSourceForTest();
    if (composed.pixelColor(80, 56).alpha() != 0) {
      error = QStringLiteral("direct clip left the green band in the source");
      return false;
    }
    if (composed.pixelColor(80, 8).alpha() == 0) {
      error = QStringLiteral("direct clip punched outside the rect");
      return false;
    }

    bool foundClip = false;
    for (const Annotation &annotation : editor.currentAnnotationsForTest()) {
      if (annotation.kind != Annotation::Kind::Clip)
        continue;
      foundClip = true;
      if (annotation.image.isNull() ||
          annotation.image.pixelColor(0, 0) != fixtureBandColor(3)) {
        error = QStringLiteral("clip tile is not the green band");
        return false;
      }
    }
    if (!foundClip) {
      error = QStringLiteral("direct clip did not add a clip layer");
      return false;
    }

    if (!saveBmp(editor.renderCurrentOutput(),
                 outputRoot + QStringLiteral("-clip-direct-actual.bmp"),
                 error) ||
        !saveGrab(editor, outputRoot + QStringLiteral("-clip-direct.png"),
                  error))
      return false;

    const int beforeUndo = editor.operationIndex();
    QTest::keyClick(&editor, Qt::Key_Z, Qt::ControlModifier);
    application.processEvents();
    if (editor.operationIndex() != beforeUndo - 1) {
      error = QStringLiteral("clip undo did not move the log cursor");
      return false;
    }
    if (editor.composedSourceForTest().pixelColor(80, 56).alpha() == 0) {
      error = QStringLiteral("clip undo left the hole");
      return false;
    }
    if (!editor.currentAnnotationsForTest().isEmpty()) {
      error = QStringLiteral("clip undo left the clip layer");
      return false;
    }
    editor.close();
  }

  // Empty Select marquee on a source with no layers becomes a pixel clip;
  // dragging that rect lifts and commits.
  {
    CaptureEditor editor(fixtureCapture(source),
                         CaptureEditor::CaptureMode::File);
    editor.resize(800, 600);
    editor.show();
    application.processEvents();
    QTest::keyClick(&editor, Qt::Key_B);
    application.processEvents();
    QTest::keyClick(&editor, Qt::Key_V);
    application.processEvents();
    if (editor.armedToolForTest() != CaptureEditor::Tool::Select) {
      error = QStringLiteral("V did not arm Select");
      return false;
    }

    const QPoint from = screenOf(editor, 10, 48);
    const QPoint to = screenOf(editor, kWidth - 10, 64);
    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&editor, to, 10);
    application.processEvents();
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, to);
    application.processEvents();
    if (editor.pixelClipRectForTest().isEmpty()) {
      error = QStringLiteral("empty marquee did not lock a pixel clip");
      return false;
    }
    if (!saveGrab(editor, outputRoot + QStringLiteral("-clip-marquee.png"),
                  error))
      return false;

    const QPoint liftFrom = screenOf(editor, kWidth / 2.0, 56);
    const QPoint liftTo = screenOf(editor, kWidth / 2.0, 120);
    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, liftFrom);
    application.processEvents();
    if (!editor.clipLiftActiveForTest()) {
      error = QStringLiteral("drag inside the pixel clip did not start a lift");
      return false;
    }
    QTest::mouseMove(&editor, liftTo, 10);
    application.processEvents();
    if (!saveGrab(editor, outputRoot + QStringLiteral("-clip-lift.png"), error))
      return false;
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, liftTo);
    application.processEvents();
    if (editor.clipLiftActiveForTest()) {
      error = QStringLiteral("clip lift did not commit on release");
      return false;
    }
    if (editor.composedSourceForTest()
            .pixelColor(kWidth / 2, 56)
            .alpha() != 0) {
      error = QStringLiteral("lifted clip left the band in the source");
      return false;
    }
    if (!saveGrab(editor, outputRoot + QStringLiteral("-clip-committed.png"),
                  error) ||
        !saveBmp(editor.renderCurrentOutput(),
                 outputRoot + QStringLiteral("-clip-committed.bmp"), error))
      return false;

    QTest::keyClick(&editor, Qt::Key_Z, Qt::ControlModifier);
    application.processEvents();
    if (!saveGrab(editor, outputRoot + QStringLiteral("-clip-undo.png"), error))
      return false;
    if (editor.composedSourceForTest().pixelColor(kWidth / 2, 56).alpha() ==
        0) {
      error = QStringLiteral("widget clip undo left the hole");
      return false;
    }
    editor.close();
  }

  // Repeat: two clips from different bands both stay as layers.
  {
    CaptureEditor editor(fixtureCapture(source),
                         CaptureEditor::CaptureMode::File);
    editor.resize(800, 600);
    editor.show();
    application.processEvents();
    editor.applyClipForTest(QRectF(0, 48, kWidth, kBand),
                            QRectF(kWidth + 8, 48, kWidth, kBand));
    editor.applyClipForTest(QRectF(0, 80, kWidth, kBand),
                            QRectF(kWidth + 8, 80, kWidth, kBand));
    int clips = 0;
    for (const Annotation &annotation : editor.currentAnnotationsForTest()) {
      if (annotation.kind == Annotation::Kind::Clip)
        ++clips;
    }
    if (clips != 2) {
      error = QStringLiteral("repeat clip did not keep both layers");
      return false;
    }
    editor.close();
  }

  return true;
}
