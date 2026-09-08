// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controls/application_icon.h"
#include <QColor>
#include <QPainter>
#include <QPixmap>

namespace QindaQt::Apps::TextEditor {
// GUI-thread icon mask over the shared public catalog. The caller supplies the
// semantic foreground; this helper never chooses an application palette.
inline QIcon editorIcon(const QString &name, const QColor &foreground) {
  const QIcon source =
      QindaQt::Controls::applicationIcon(name + QStringLiteral("-symbolic"));
  QIcon result;
  for (const int size : {16, 22, 24, 32, 48, 64}) {
    QPixmap pixmap = source.pixmap(size, size);
    if (pixmap.isNull())
      continue;
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), foreground);
    painter.end();
    result.addPixmap(pixmap);
  }
  return result;
}
} // namespace QindaQt::Apps::TextEditor
