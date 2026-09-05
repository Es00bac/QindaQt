// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>

#include <QtQml/qqmlregistration.h>

#include <memory>

class QQmlEngine;
class QJSEngine;

namespace QindaQt::Shell::Icons
{

class IconThemeLocator;

// AGENT-CONTRACT: Per-engine QML singleton backing the `Icon` element's
// resolved/fallback decision. It answers the same confined lookup the
// `image://qindaqt-icon/` provider performs so QML can choose the typed
// fallback glyph without issuing a provider request. The locator is
// injected exclusively through IconRuntime::install(); without installation
// every query fails closed to "unresolved". GUI-thread only, like every QML
// singleton.
class IconLookup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    QML_UNCREATABLE("IconLookup is installed by IconRuntime::install")

public:
    // Qt singleton factory: one instance per engine, owned by the engine.
    static IconLookup *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    // Same validation, chain order, and confinement as the image provider.
    Q_INVOKABLE [[nodiscard]] bool hasIcon(const QString &name, int size, double scale,
                                           bool symbolic) const;

    // Composition seam used by IconRuntime::install() only; not QML-visible.
    static void installForEngine(QQmlEngine *engine,
                                 std::shared_ptr<IconThemeLocator> locator);

private:
    explicit IconLookup(std::shared_ptr<IconThemeLocator> locator, QQmlEngine *parent);

    std::shared_ptr<IconThemeLocator> m_locator;
};

} // namespace QindaQt::Shell::Icons
