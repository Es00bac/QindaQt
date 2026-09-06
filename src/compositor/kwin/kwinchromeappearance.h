// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QDBusConnection>
#include <QObject>
#include <QSet>
#include <memory>

namespace QindaQt::AppAppearance {
class ApplicationAppearanceController;
}
namespace QindaQt::Services::SettingsClient {
class QtSettingsTransport;
class SettingsClient;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

// Owns one Settings1 appearance subscription for the compositor process and
// republishes its validated palette to grouped and native chrome.
class KWinChromeAppearance final : public QObject {
  Q_OBJECT
public:
  explicit KWinChromeAppearance(ManagedWindowRegistry &registry,
                                QDBusConnection bus, QObject *parent = nullptr);
  ~KWinChromeAppearance() override;

  [[nodiscard]] HybridChrome::ChromePalette palette() const {
    return m_palette;
  }

Q_SIGNALS:
  void paletteChanged(const QindaQt::HybridChrome::ChromePalette &palette);

private:
  void publish();
  void observeWindow(const QString &windowId);
  void publishWindow(const QString &windowId);

  ManagedWindowRegistry &m_registry;
  std::unique_ptr<Services::SettingsClient::QtSettingsTransport> m_transport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_settings;
  std::unique_ptr<AppAppearance::ApplicationAppearanceController> m_appearance;
  HybridChrome::ChromePalette m_palette;
  QSet<QString> m_observedWindows;
};

} // namespace QindaQt::Compositor::KWinIntegration
