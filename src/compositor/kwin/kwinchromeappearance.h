// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/themes/decoration_theme_spec.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QDBusConnection>
#include <QObject>
#include <QPalette>
#include <QSet>
#include <QVariantMap>
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
  Q_PROPERTY(QVariantMap qmlPalette READ qmlPalette NOTIFY qmlPaletteChanged)
public:
  explicit KWinChromeAppearance(ManagedWindowRegistry &registry,
                                QDBusConnection bus, QObject *parent = nullptr);
  ~KWinChromeAppearance() override;

  [[nodiscard]] HybridChrome::ChromePalette palette() const {
    return m_palette;
  }
  [[nodiscard]] QPalette nativePalette() const { return m_nativePalette; }
  // Container chrome arrangement and palette for the resolved theme and the
  // user's chrome preferences (ADR-0129).
  [[nodiscard]] HybridChrome::ChromeStyle containerStyle() const {
    return m_containerStyle;
  }
  [[nodiscard]] QVariantMap qmlPalette() const { return m_qmlPalette; }

Q_SIGNALS:
  // ADR-0264: a QindaQt decoration's roll-up button or roll-up title
  // double-click asked to roll this managed window up to its icon.
  void windowRollUpRequested(const QString &windowId);
  void paletteChanged(const QindaQt::HybridChrome::ChromePalette &palette);
  void nativePaletteChanged(const QPalette &palette);
  void containerStyleChanged(const QindaQt::HybridChrome::ChromeStyle &style);
  void qmlPaletteChanged();

private Q_SLOTS:
  void relayDecorationRollUp();

private:
  void publish();
  void refreshPreferences();
  void refreshDecorationPreferences();
  void observeWindow(const QString &windowId);
  void publishWindow(const QString &windowId);

  ManagedWindowRegistry &m_registry;
  std::unique_ptr<Services::SettingsClient::QtSettingsTransport> m_transport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_settings;
  std::unique_ptr<AppAppearance::ApplicationAppearanceController> m_appearance;
  std::unique_ptr<Services::SettingsClient::QtSettingsTransport>
      m_preferencesTransport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_preferencesSettings;
  // Decoration document choices (ADR-0207) ride a third purpose-scoped
  // client so an older Settings1 schema costs only the pairing.
  std::unique_ptr<Services::SettingsClient::QtSettingsTransport>
      m_decorationTransport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_decorationSettings;
  QVector<Themes::DecorationThemeSpec> m_decorations;
  Decoration::ChromePreferences m_preferences;
  HybridChrome::ChromeStyle m_containerStyle =
      HybridChrome::ChromeStyle::qindaMacOS({});
  HybridChrome::ChromePalette m_palette;
  QPalette m_nativePalette;
  QVariantMap m_qmlPalette;
  QSet<QString> m_observedWindows;
};

} // namespace QindaQt::Compositor::KWinIntegration
