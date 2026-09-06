// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromeappearance.h"

#include "chromeappearancepalette.h"
#include "managedwindowregistry.h"
#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <KDecoration3/Decoration>
#include <window.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {
constexpr auto PaletteProperty = "qindaqtChromePalette";
}

KWinChromeAppearance::KWinChromeAppearance(ManagedWindowRegistry &registry,
                                           QDBusConnection bus, QObject *parent)
    : QObject(parent), m_registry(registry) {
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
  connect(&m_registry, &ManagedWindowRegistry::managedWindowAdded, this,
          &KWinChromeAppearance::observeWindow);
  for (const auto &id : m_registry.windowIds())
    observeWindow(id);
  QString error;
  if (!m_settings->start(&error))
    qWarning("QindaQt chrome appearance could not start Settings1: %s",
             qPrintable(error));
  publish();
}

KWinChromeAppearance::~KWinChromeAppearance() = default;

void KWinChromeAppearance::publish() {
  m_palette = chromePaletteForTheme(m_appearance->theme());
  for (const auto &id : m_registry.windowIds())
    publishWindow(id);
  Q_EMIT paletteChanged(m_palette);
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
    decoration->setProperty(PaletteProperty,
                            decorationPaletteProperties(m_palette));
    decoration->update();
  }
}

} // namespace QindaQt::Compositor::KWinIntegration
