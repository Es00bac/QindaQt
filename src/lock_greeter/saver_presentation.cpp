// SPDX-License-Identifier: GPL-3.0-or-later
#include "saver_presentation.h"
namespace QindaQt::LockGreeter {
SaverPresentation::SaverPresentation(Session::DesktopControls::ScreensaverPreferencesProvider &provider)
    : m_provider(provider) {
  connect(&provider, &Session::DesktopControls::ScreensaverPreferencesProvider::preferencesChanged,
          this, [this] { refresh(); });
  refresh();
}
void SaverPresentation::refresh() {
  const auto token = m_provider.currentPreferences().saver;
  QUrl scene;
  if (token == QStringLiteral("qinda-patrol")) scene = QUrl(QStringLiteral("qrc:/native-lock/PatrolScene.qml"));
  else if (token == QStringLiteral("circuit-reef")) scene = QUrl(QStringLiteral("qrc:/native-lock/ReefScene.qml"));
  if (scene == m_scene) return;
  m_scene = scene; Q_EMIT changed();
}
}
