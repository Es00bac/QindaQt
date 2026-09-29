// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QUrl>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>
namespace QindaQt::LockGreeter {
// Pure presentation selector borrowing a purpose-scoped public preference
// provider. Persisted tokens choose fixed compiled scene URLs, never QML code.
class SaverPresentation final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QUrl scene READ scene NOTIFY changed)
public:
  explicit SaverPresentation(Session::DesktopControls::ScreensaverPreferencesProvider &provider);
  QUrl scene() const { return m_scene; }
Q_SIGNALS:
  void changed();
private:
  void refresh();
  Session::DesktopControls::ScreensaverPreferencesProvider &m_provider;
  QUrl m_scene;
};
}
