// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QQuickImageProvider>

#include <QHash>
#include <QImage>

namespace QindaQt::ShellTaskListApplet {

// AGENT-CONTRACT: bridges controller-owned preview QImages into QML. The
// controller inserts every captured preview under a monotonic token and hands
// the token to presentation; QML addresses the image as
// "image://qindaqt-task-preview/<token>". Tokens are never reused, so a
// stale Image source can never show a newer window's pixels. The cache keeps
// only the last few previews (bounded memory for hostile hover patterns).
class TaskListPreviewProvider final : public QQuickImageProvider {
public:
  static constexpr char kProviderName[] = "qindaqt-task-preview";
  static constexpr int kMaxCachedPreviews = 4;

  TaskListPreviewProvider();

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requestedSize) override;

  // Stores the image and returns its token (monotonic from 1; 0 is reserved
  // for "unavailable").
  int insert(const QImage &image);

private:
  QHash<int, QImage> m_images;
  int m_nextToken = 1;
};

} // namespace QindaQt::ShellTaskListApplet
