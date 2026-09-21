// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library_controller.h"

#include <QQuickImageProvider>

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
    const QImage image = m_controller->imageForGame(id);
    if (size != nullptr) {
      *size = image.size();
    }
    return image;
  }

private:
  LibraryController *m_controller; // borrowed; owned by main()
};

} // namespace QindaQt::QindaLutris
