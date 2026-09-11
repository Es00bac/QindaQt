// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QPalette;
class QStyle;

namespace QindaQt::Apps::SettingsAppearance {

// AGENT-CONTRACT: The Appearance preview window (ADR-0127). Title bars,
// frames, and buttons are painted by the shared decoration painter that the
// KDecoration plugin uses for live windows, and the client area is painted
// by the real Fusion QStyle with the palette ordinary Qt applications
// receive, so this item shows a theme as it will look rather than a mock.
// Inputs are plain values published by the route model; the item owns no
// settings client and never reads the live session.
class AppearanceWindowPreview : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppearanceWindowPreview)
    // The compositor's flattened decoration chrome for the previewed theme.
    Q_PROPERTY(QVariantMap chrome READ chrome WRITE setChrome NOTIFY chromeChanged)
    // QPalette roles ("window", "windowText", "base", "text", "button",
    // "buttonText", "highlight", "highlightedText", "mid", "dark", "light",
    // "placeholderText", "disabledText") as colors.
    Q_PROPERTY(QVariantMap toolkitPalette READ toolkitPalette WRITE setToolkitPalette
                   NOTIFY toolkitPaletteChanged)
    Q_PROPERTY(QFont toolkitFont READ toolkitFont WRITE setToolkitFont
                   NOTIFY toolkitFontChanged)
    Q_PROPERTY(QColor canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    Q_PROPERTY(QString caption READ caption WRITE setCaption NOTIFY captionChanged)
    Q_PROPERTY(bool showInactiveWindow READ showInactiveWindow
                   WRITE setShowInactiveWindow NOTIFY showInactiveWindowChanged)
    // True when the client area paints through the real QStyle; false when
    // no widgets application exists (headless tests) and a flat palette
    // rendering stands in.
    Q_PROPERTY(bool nativeStyleAvailable READ nativeStyleAvailable CONSTANT)

public:
    explicit AppearanceWindowPreview(QQuickItem *parent = nullptr);
    ~AppearanceWindowPreview() override;

    [[nodiscard]] QVariantMap chrome() const { return m_chrome; }
    void setChrome(const QVariantMap &chrome);
    [[nodiscard]] QVariantMap toolkitPalette() const { return m_toolkitPalette; }
    void setToolkitPalette(const QVariantMap &palette);
    [[nodiscard]] QFont toolkitFont() const { return m_toolkitFont; }
    void setToolkitFont(const QFont &font);
    [[nodiscard]] QColor canvas() const { return m_canvas; }
    void setCanvas(const QColor &canvas);
    [[nodiscard]] QString caption() const { return m_caption; }
    void setCaption(const QString &caption);
    [[nodiscard]] bool showInactiveWindow() const { return m_showInactiveWindow; }
    void setShowInactiveWindow(bool show);
    [[nodiscard]] bool nativeStyleAvailable() const;

    void paint(QPainter *painter) override;

    // Exposed for tests: the palette rebuilt from toolkitPalette.
    [[nodiscard]] QPalette resolvedPalette() const;

Q_SIGNALS:
    void chromeChanged();
    void toolkitPaletteChanged();
    void toolkitFontChanged();
    void canvasChanged();
    void captionChanged();
    void showInactiveWindowChanged();

private:
    void paintWindow(QPainter &painter, const QRectF &frame, const QString &caption,
                     bool active) const;
    void paintToolkitSample(QPainter &painter, const QRectF &area, bool active) const;
    void paintFlatSample(QPainter &painter, const QRectF &area, const QPalette &palette) const;

    QVariantMap m_chrome;
    QVariantMap m_toolkitPalette;
    QFont m_toolkitFont;
    QColor m_canvas;
    QString m_caption = QStringLiteral("QindaQt Settings");
    bool m_showInactiveWindow = true;
    QStyle *m_style = nullptr;
};

} // namespace QindaQt::Apps::SettingsAppearance
