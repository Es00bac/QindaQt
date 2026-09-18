// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QQuickPaintedItem>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace QindaQt::Apps::SettingsAppearance {

// AGENT-CONTRACT: The Appearance container preview (ADR-0129). A two-member
// container is laid out by the compositor's own ChromeLayoutEngine and
// painted by its ChromeRenderer, and each member's native title bar is
// painted by the shared decoration painter, so both chrome sets appear
// exactly as the compositor composes a grouped window. Inputs are plain
// values from the route model; the item owns no settings client.
class AppearanceContainerPreview : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppearanceContainerPreview)
    // containerStyleToVariantMap() shape: arrangement plus chrome palette.
    Q_PROPERTY(QVariantMap containerStyle READ containerStyle WRITE setContainerStyle
                   NOTIFY containerStyleChanged)
    // The window decoration map member title bars paint from.
    Q_PROPERTY(QVariantMap chrome READ chrome WRITE setChrome NOTIFY chromeChanged)
    Q_PROPERTY(QVariantMap toolkitPalette READ toolkitPalette WRITE setToolkitPalette
                   NOTIFY toolkitPaletteChanged)
    Q_PROPERTY(QFont toolkitFont READ toolkitFont WRITE setToolkitFont
                   NOTIFY toolkitFontChanged)
    Q_PROPERTY(QColor canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    // A local wallpaper file painted behind the windows (cover-scaled) so a
    // translucent material previews against the desktop it will sit on.
    Q_PROPERTY(QUrl wallpaper READ wallpaper WRITE setWallpaper NOTIFY wallpaperChanged)

public:
    explicit AppearanceContainerPreview(QQuickItem *parent = nullptr);

    [[nodiscard]] QVariantMap containerStyle() const { return m_containerStyle; }
    void setContainerStyle(const QVariantMap &style);
    [[nodiscard]] QVariantMap chrome() const { return m_chrome; }
    void setChrome(const QVariantMap &chrome);
    [[nodiscard]] QVariantMap toolkitPalette() const { return m_toolkitPalette; }
    void setToolkitPalette(const QVariantMap &palette);
    [[nodiscard]] QFont toolkitFont() const { return m_toolkitFont; }
    void setToolkitFont(const QFont &font);
    [[nodiscard]] QColor canvas() const { return m_canvas; }
    void setCanvas(const QColor &canvas);
    [[nodiscard]] QUrl wallpaper() const { return m_wallpaper; }
    void setWallpaper(const QUrl &wallpaper);
    // True when a wallpaper image is loaded and painted.
    [[nodiscard]] bool wallpaperLoaded() const { return !m_wallpaperImage.isNull(); }

    // The container request this item paints at its current size.
    [[nodiscard]] HybridChrome::ChromeLayoutRequest layoutRequest() const;
    // True when the compositor's layout engine accepts that request.
    Q_INVOKABLE bool layoutBuilds() const;

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void containerStyleChanged();
    void chromeChanged();
    void toolkitPaletteChanged();
    void toolkitFontChanged();
    void canvasChanged();
    void wallpaperChanged();

private:
    QVariantMap m_containerStyle;
    QVariantMap m_chrome;
    QVariantMap m_toolkitPalette;
    QFont m_toolkitFont;
    QColor m_canvas;
    QUrl m_wallpaper;
    QImage m_wallpaperImage;
};

} // namespace QindaQt::Apps::SettingsAppearance
