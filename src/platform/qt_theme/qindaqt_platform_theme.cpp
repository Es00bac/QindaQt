// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt_platform_theme.h"
#include <qindaqt/app_appearance/application_appearance_controller.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/themes/theme_loader.h>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qwindowsysteminterface.h>

namespace QindaQt::QtTheme {
using namespace Services::SettingsClient;
class AppearanceBinding final {
public:
    explicit AppearanceBinding(const QStringList &directories)
        : transport(QDBusConnection::sessionBus()),
          client(transport, {QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme"),
                            QStringLiteral("fonts.family"), QStringLiteral("fonts.monospaceFamily"),
                            QStringLiteral("fonts.pointSize"), QStringLiteral("accessibility.textScale"),
                            QStringLiteral("accessibility.highContrast"), QStringLiteral("accessibility.reducedMotion"),
                            QStringLiteral("accessibility.reducedTransparency")}),
          controller(client, directories, QStringLiteral("qinda-dark")) {}
    QtSettingsTransport transport;
    SettingsClient client;
    AppAppearance::ApplicationAppearanceController controller;
};

PlatformTheme::PlatformTheme()
    : PlatformTheme(std::unique_ptr<QPlatformTheme>(QGuiApplicationPrivate::platformIntegration()->createPlatformTheme(QStringLiteral("generic"))),
                    AppAppearance::standardThemeDirectories()) {}

PlatformTheme::PlatformTheme(std::unique_ptr<QPlatformTheme> base, QStringList directories)
    : m_base(std::move(base)), m_directories(std::move(directories))
{
    if (!m_base) m_base = std::make_unique<QPlatformTheme>();
    for (const auto &directory : std::as_const(m_directories)) {
        const auto loaded = Themes::ThemeLoader::fromFile(QDir(directory).filePath(QStringLiteral("qinda-dark.json")));
        if (loaded.ok) {
            m_appearance = nativeAppearance(loaded.theme, {});
            break;
        }
    }
    // AGENT-GUARD: QPA construction precedes QApplication's style hints and
    // palette initialization. Start the async public client only afterwards;
    // never spin a nested event loop or read Settings1 persistence here.
    QTimer::singleShot(0, this, &PlatformTheme::startSettings);
}
PlatformTheme::~PlatformTheme() = default;

void PlatformTheme::startSettings()
{
    m_binding = std::make_unique<AppearanceBinding>(m_directories);
    connect(&m_binding->controller, &AppAppearance::ApplicationAppearanceController::appearanceChanged,
            this, &PlatformTheme::updateAppearance);
    updateAppearance();
    // A missing service is ordinary startup/loss. The public client retries
    // and retains the last confirmed value; this adapter never writes settings.
    QString error;
    (void)m_binding->client.start(&error);
}
void PlatformTheme::updateAppearance()
{
    auto next = nativeAppearance(m_binding->controller.theme(), m_binding->controller.accessibilityInputs());
    if (!next) return;
    if (m_appearance && next->palette == m_appearance->palette
        && next->font == m_appearance->font && next->fixedFont == m_appearance->fixedFont
        && next->iconTheme == m_appearance->iconTheme && next->scheme == m_appearance->scheme
        && next->highContrast == m_appearance->highContrast) return;
    m_appearance = std::move(next);
    QWindowSystemInterface::handleThemeChange();
}
const QPalette *PlatformTheme::palette(Palette type) const
{
    return m_appearance ? &m_appearance->palette : m_base->palette(type);
}
const QFont *PlatformTheme::font(Font type) const
{
    if (!m_appearance) return m_base->font(type);
    return type == FixedFont ? &m_appearance->fixedFont : &m_appearance->font;
}
QVariant PlatformTheme::themeHint(ThemeHint hint) const
{
    if (hint == StyleNames) return QStringList{QStringLiteral("Fusion")};
    if (hint == SystemIconThemeName && m_appearance) return m_appearance->iconTheme;
    if (hint == SystemIconFallbackThemeName) return QStringLiteral("hicolor");
    if (hint == IconThemeSearchPaths) {
        QStringList paths;
        for (const auto &directory : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
            paths.append(QDir(directory).filePath(QStringLiteral("icons")));
        return paths;
    }
    return m_base->themeHint(hint);
}
Qt::ColorScheme PlatformTheme::colorScheme() const
{
    return m_appearance ? m_appearance->scheme : m_base->colorScheme();
}
Qt::ContrastPreference PlatformTheme::contrastPreference() const
{
    return m_appearance && m_appearance->highContrast ? Qt::ContrastPreference::HighContrast
                                                    : Qt::ContrastPreference::NoPreference;
}
// AGENT-CONTRACT: Appearance does not replace native platform services. Ask
// Qt's integration for its generic theme directly (no plugin-factory lookup,
// which can recursively rediscover qindaqt via XDG_CURRENT_DESKTOP).
QPlatformSystemTrayIcon *PlatformTheme::createPlatformSystemTrayIcon() const { return m_base->createPlatformSystemTrayIcon(); }
QPlatformMenuItem *PlatformTheme::createPlatformMenuItem() const { return m_base->createPlatformMenuItem(); }
QPlatformMenu *PlatformTheme::createPlatformMenu() const { return m_base->createPlatformMenu(); }
QPlatformMenuBar *PlatformTheme::createPlatformMenuBar() const { return m_base->createPlatformMenuBar(); }
bool PlatformTheme::usePlatformNativeDialog(DialogType type) const { return m_base->usePlatformNativeDialog(type); }
QPlatformDialogHelper *PlatformTheme::createPlatformDialogHelper(DialogType type) const { return m_base->createPlatformDialogHelper(type); }
}
