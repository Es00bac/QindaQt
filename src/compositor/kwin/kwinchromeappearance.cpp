// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromeappearance.h"

#include "chromeappearancepalette.h"
#include "managedwindowregistry.h"
#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/decoration_theme_loader.h"

#include <KDecoration3/Decoration>
#include <QtQml/qqml.h>
#include <window.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {
constexpr auto PaletteProperty = "qindaqtChromePalette";
}

KWinChromeAppearance::KWinChromeAppearance(ManagedWindowRegistry &registry,
                                           QDBusConnection bus, QObject *parent)
    : QObject(parent), m_registry(registry) {
  qmlRegisterSingletonInstance("QindaQt.Compositor.Appearance", 1, 0,
                               "Appearance", this);
  m_transport =
      std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus);
  m_settings = std::make_unique<Services::SettingsClient::SettingsClient>(
      *m_transport, QStringList{QStringLiteral("appearance.theme"),
                                QStringLiteral("appearance.colorScheme")});
  m_appearance =
      std::make_unique<AppAppearance::ApplicationAppearanceController>(
          *m_settings, AppAppearance::standardThemeDirectories(),
          QStringLiteral("qinda-dark"));
  connect(m_appearance.get(),
          &AppAppearance::ApplicationAppearanceController::appearanceChanged,
          this, &KWinChromeAppearance::publish);
  // AGENT-CONTRACT: chrome arrangement rides its own purpose-scoped client
  // (ADR-0129). Settings1 rejects a whole scoped snapshot when any key is
  // unknown, so an older resident service can cost only the arrangement,
  // never the theme.
  m_preferencesTransport =
      std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus);
  m_preferencesSettings =
      std::make_unique<Services::SettingsClient::SettingsClient>(
          *m_preferencesTransport,
          Decoration::ChromePreferences::settingsKeys());
  connect(m_preferencesSettings.get(),
          &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &KWinChromeAppearance::refreshPreferences);
  QString decorationError;
  if (auto loaded = Themes::DecorationThemeLoader::loadDirectories(
          AppAppearance::standardDecorationDirectories(), &decorationError))
    m_decorations = *loaded;
  else
    qWarning("QindaQt decoration documents unavailable: %s",
             qPrintable(decorationError));
  m_decorationTransport =
      std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus);
  m_decorationSettings =
      std::make_unique<Services::SettingsClient::SettingsClient>(
          *m_decorationTransport,
          Decoration::ChromePreferences::decorationKeys());
  connect(m_decorationSettings.get(),
          &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &KWinChromeAppearance::refreshDecorationPreferences);
  connect(&m_registry, &ManagedWindowRegistry::managedWindowAdded, this,
          &KWinChromeAppearance::observeWindow);
  for (const auto &id : m_registry.windowIds())
    observeWindow(id);
  QString error;
  if (!m_settings->start(&error))
    qWarning("QindaQt chrome appearance could not start Settings1: %s",
             qPrintable(error));
  error.clear();
  if (!m_preferencesSettings->start(&error))
    qWarning("QindaQt chrome preferences could not start Settings1: %s",
             qPrintable(error));
  error.clear();
  if (!m_decorationSettings->start(&error))
    qWarning("QindaQt decoration pairing could not start Settings1: %s",
             qPrintable(error));
  publish();
}

KWinChromeAppearance::~KWinChromeAppearance() = default;

void KWinChromeAppearance::refreshPreferences() {
  const auto &snapshot = m_preferencesSettings->snapshot();
  auto next = snapshot
      ? Decoration::ChromePreferences::fromSettingsValues(snapshot->values)
      : Decoration::ChromePreferences{};
  // The arrangement snapshot never carries the pairing keys; keep them.
  next.windowDecoration = m_preferences.windowDecoration;
  next.containerDecoration = m_preferences.containerDecoration;
  if (next == m_preferences)
    return;
  m_preferences = next;
  publish();
}

void KWinChromeAppearance::refreshDecorationPreferences() {
  const auto &snapshot = m_decorationSettings->snapshot();
  const auto pairing = snapshot
      ? Decoration::ChromePreferences::fromSettingsValues(snapshot->values)
      : Decoration::ChromePreferences{};
  if (pairing.windowDecoration == m_preferences.windowDecoration &&
      pairing.containerDecoration == m_preferences.containerDecoration)
    return;
  m_preferences.windowDecoration = pairing.windowDecoration;
  m_preferences.containerDecoration = pairing.containerDecoration;
  publish();
}

void KWinChromeAppearance::publish() {
  const auto &theme = m_appearance->theme();
  m_palette = chromePaletteForTheme(theme);
  m_nativePalette = nativePaletteForTheme(theme);
  // ADR-0207: the theme names its decoration pairing; the user's explicit
  // choice wins when that document is installed.
  const auto windowDocument = Decoration::selectDecorationTheme(
      theme, m_decorations, m_preferences.windowDecoration);
  const auto containerDocument = Decoration::selectDecorationTheme(
      theme, m_decorations, m_preferences.containerDecoration);
  m_qmlPalette = decorationPaletteProperties(m_palette, theme, windowDocument,
                                             m_preferences);
  m_containerStyle =
      Decoration::resolveContainerStyle(theme, containerDocument, m_preferences);
  m_qmlPalette.insert(QStringLiteral("accent"),
                      m_nativePalette.color(QPalette::Highlight));
  m_qmlPalette.insert(QStringLiteral("accentText"),
                      m_nativePalette.color(QPalette::HighlightedText));
  for (const auto &id : m_registry.windowIds())
    publishWindow(id);
  Q_EMIT paletteChanged(m_palette);
  Q_EMIT containerStyleChanged(m_containerStyle);
  Q_EMIT nativePaletteChanged(m_nativePalette);
  Q_EMIT qmlPaletteChanged();
}

void KWinChromeAppearance::observeWindow(const QString &windowId) {
  auto *window = m_registry.window(windowId);
  if (!window || m_observedWindows.contains(windowId))
    return;
  m_observedWindows.insert(windowId);
  connect(window, &KWin::Window::decorationChanged, this,
          [this, windowId] { publishWindow(windowId); });
  publishWindow(windowId);
}

void KWinChromeAppearance::publishWindow(const QString &windowId) {
  auto *window = m_registry.window(windowId);
  if (!window)
    return;
  if (auto *decoration = window->decoration()) {
    // AGENT-CONTRACT: QindaDecoration consumes this process-local map;
    // foreign decorations continue to use their KDecoration palette.
    decoration->setProperty(PaletteProperty, m_qmlPalette);
    decoration->update();
    // AGENT-CONTRACT (ADR-0264): QindaDecoration declares
    // qindaqtRollUpRequested(); a foreign decoration does not and is never
    // connected. Unique, because every palette publish passes through here.
    if (decoration->metaObject()->indexOfSignal("qindaqtRollUpRequested()") >= 0)
      connect(decoration, SIGNAL(qindaqtRollUpRequested()), this,
              SLOT(relayDecorationRollUp()), Qt::UniqueConnection);
  }
}

void KWinChromeAppearance::relayDecorationRollUp() {
  const QObject *decoration = sender();
  for (const auto &id : m_registry.windowIds()) {
    auto *window = m_registry.window(id);
    if (window && static_cast<const QObject *>(window->decoration()) == decoration) {
      Q_EMIT windowRollUpRequested(id);
      return;
    }
  }
}

} // namespace QindaQt::Compositor::KWinIntegration
