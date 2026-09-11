// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "native_palette.h"
#include <QObject>
#include <qpa/qplatformtheme.h>
#include <functional>
#include <memory>

namespace QindaQt::QtTheme {
class AppearanceBinding;
// Private QPA boundary: GUI-thread owned for the lifetime of QApplication.
// Qt borrows the returned palette/font pointers. Storage remains stable until
// replacement followed by Qt's ordinary theme-change notification.
class PlatformTheme final : public QObject, public QPlatformTheme {
    Q_OBJECT
public:
    // Answers, at the moment of a QMenuBar's creation, whether a global-menu
    // host currently owns the AppMenu registrar. Called on the GUI thread.
    using GlobalMenuHostProbe = std::function<bool()>;
    PlatformTheme();
    // Constructor-injected platform services and host probe are owned for the
    // theme lifetime. An empty probe means "no host": menubars stay in-window.
    PlatformTheme(std::unique_ptr<QPlatformTheme> base, QStringList directories,
                  GlobalMenuHostProbe globalMenuHostPresent);
    ~PlatformTheme() override;
    // Production probe: one synchronous bus-daemon NameHasOwner query for
    // com.canonical.AppMenu.Registrar on the session bus; false without a bus.
    [[nodiscard]] static bool appMenuRegistrarOwned();
    const QPalette *palette(Palette type = SystemPalette) const override;
    const QFont *font(Font type = SystemFont) const override;
    QVariant themeHint(ThemeHint hint) const override;
    Qt::ColorScheme colorScheme() const override;
    Qt::ContrastPreference contrastPreference() const override;
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override;
    QPlatformMenuItem *createPlatformMenuItem() const override;
    QPlatformMenu *createPlatformMenu() const override;
    // AGENT-CONTRACT (ADR-0130): decided per QMenuBar creation. A D-Bus
    // menubar (QMenuBar hides itself) only while the probe reports a live
    // registrar owner; otherwise nullptr keeps QMenuBar in its window. The
    // shell owns that name only while its layout hosts a global-menu applet.
    // A menubar already created keeps its mode until the application
    // recreates it or restarts.
    QPlatformMenuBar *createPlatformMenuBar() const override;
    bool usePlatformNativeDialog(DialogType type) const override;
    QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const override;
private:
    void startSettings();
    void updateAppearance();
    std::unique_ptr<QPlatformTheme> m_base;
    QStringList m_directories;
    std::optional<NativeAppearance> m_appearance;
    std::unique_ptr<AppearanceBinding> m_binding;
    GlobalMenuHostProbe m_globalMenuHostPresent;
};
}
