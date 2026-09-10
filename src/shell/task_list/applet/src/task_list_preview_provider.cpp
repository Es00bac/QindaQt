// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_preview_provider.h"

namespace QindaQt::ShellTaskListApplet {

constexpr char TaskListPreviewProvider::kProviderName[];

TaskListPreviewProvider::TaskListPreviewProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage TaskListPreviewProvider::requestImage(const QString &id, QSize *size,
                                             const QSize &requestedSize) {
  const int token = id.toInt();
  const QImage image = m_images.value(token);
  if (size != nullptr) {
    size->setWidth(image.width());
    size->setHeight(image.height());
  }
  Q_UNUSED(requestedSize);
  return image;
}

int TaskListPreviewProvider::insert(const QImage &image) {
  if (image.isNull()) {
    return 0;
  }
  const int token = m_nextToken;
  if (m_nextToken == INT_MAX) {
    m_nextToken = 1;
  } else {
    ++m_nextToken;
  }
  m_images.insert(token, image);
  while (m_images.size() > kMaxCachedPreviews) {
    // Drop the oldest entry (QHash iteration order is unspecified, so track
    // the floor explicitly instead of iterating).
    int oldest = 0;
    for (auto it = m_images.constBegin(); it != m_images.constEnd(); ++it) {
      if (oldest == 0 || it.key() < oldest) {
        oldest = it.key();
      }
    }
    m_images.remove(oldest);
    if (oldest == token) {
      break;
    }
  }
  return token;
}

} // namespace QindaQt::ShellTaskListApplet
