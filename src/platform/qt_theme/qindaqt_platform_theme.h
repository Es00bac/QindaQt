// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "native_palette.h"
#include <QObject>
#include <qpa/qplatformtheme.h>
#include <memory>

namespace QindaQt::QtTheme {
class AppearanceBinding;
// Private QPA boundary: GUI-thread owned for the lifetime of QApplication.
// Qt borrows the returned palette/font pointers. Storage remains stable until
// replacement followed by Qt's ordinary theme-change notification.
class PlatformTheme final : public QObject, public QPlatformTheme {
    Q_OBJECT
public:
    PlatformTheme();
    // Constructor-injected platform services are owned for the theme lifetime.
    PlatformTheme(std::unique_ptr<QPlatformTheme> base, QStringList directories);
    ~PlatformTheme() override;
    const QPalette *palette(Palette type = SystemPalette) const override;
    const QFont *font(Font type = SystemFont) const override;
    QVariant themeHint(ThemeHint hint) const override;
    Qt::ColorScheme colorScheme() const override;
    Qt::ContrastPreference contrastPreference() const override;
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override;
    QPlatformMenuItem *createPlatformMenuItem() const override;
    QPlatformMenu *createPlatformMenu() const override;
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
};
}
