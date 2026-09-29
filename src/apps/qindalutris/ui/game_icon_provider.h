// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library_controller.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QQuickImageProvider>
#include <QSemaphore>
#include <QThread>

#include <memory>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: image://gameicon/<game id> resolves a game's cover art,
// else its desktop-entry theme icon, else a null image (Tk.Thumbnail then
// draws its placeholder glyph -- its documented contract). Decoding is
// allocation-capped in the controller, so hostile art fails rather than the
// session. The id carries a slash ("steam/123"); QQuickImageProvider passes
// everything after the prefix as the id, which is exactly our Game::id.
class GameIconProvider final : public QQuickImageProvider {
public:
  explicit GameIconProvider(LibraryController *controller)
      : QQuickImageProvider(QQuickImageProvider::Image),
        m_controller(controller) {}

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requestedSize) override {
    Q_UNUSED(requestedSize);
    const QImage image = imageOnGuiThread(id);
    if (size != nullptr) {
      *size = image.size();
    }
    return image;
  }

private:
  // AGENT-GUARD: Qt Quick calls requestImage() on its image-reader thread for
  // asynchronous Images. The controller's game list is mutated on the GUI
  // thread during a refresh, and the theme-icon fallback uses QIcon, whose
  // caches are not thread safe (the same race crashed File Manager,
  // 2026-09-28). The lookup runs on the GUI thread; the reader waits a bounded
  // time, so an exiting application never deadlocks with it, and a destroyed
  // controller simply drops the queued call.
  static constexpr int guiThreadWaitMs = 2000;

  [[nodiscard]] QImage imageOnGuiThread(const QString &id) const {
    QCoreApplication *const application = QCoreApplication::instance();
    if (application == nullptr || QThread::currentThread() == application->thread()) {
      return m_controller->imageForGame(id);
    }
    struct Handoff final {
      QSemaphore done;
      QImage image;
    };
    const auto handoff = std::make_shared<Handoff>();
    LibraryController *const controller = m_controller;
    static_cast<void>(QMetaObject::invokeMethod(
        controller,
        [handoff, controller, id] {
          handoff->image = controller->imageForGame(id);
          handoff->done.release();
        },
        Qt::QueuedConnection));
    if (!handoff->done.tryAcquire(1, guiThreadWaitMs)) {
      return {};
    }
    return handoff->image;
  }

  LibraryController *m_controller; // borrowed; owned by main()
};

} // namespace QindaQt::QindaLutris
