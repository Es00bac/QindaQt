// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace QindaQt::Apps::SettingsCustomize {

// AGENT-CONTRACT: Paints one contained-window member handlebar (ADR-0131)
// from an already-resolved chrome map -- the same DecorationChrome shape
// CustomizeWindowPreview publishes and the compositor's own decorations
// paint from. This item owns no theme/Settings1 resolution and no second
// decoration engine: it only calls the shared decoration_painter functions
// the live KDecoration plugin and the Appearance container preview already
// call, so the Customize canvas can never show a chrome the compositor
// would not actually paint.
class CustomizeContainedWindowPreview : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(CustomizeContainedWindowPreview)
    Q_PROPERTY(QVariantMap chrome READ chrome WRITE setChrome NOTIFY chromeChanged)

public:
    explicit CustomizeContainedWindowPreview(QQuickItem *parent = nullptr);

    [[nodiscard]] QVariantMap chrome() const { return m_chrome; }
    void setChrome(const QVariantMap &chrome);

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void chromeChanged();

private:
    QVariantMap m_chrome;
};

} // namespace QindaQt::Apps::SettingsCustomize
