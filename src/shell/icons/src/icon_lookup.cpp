// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_lookup.h>

#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QQmlEngine>

namespace QindaQt::Shell::Icons
{
namespace
{

// AGENT-NOTE: One registry entry per engine keeps the injected locator
// reachable from the singleton factory Qt invokes per engine. Entries are
// removed when the engine is destroyed; GUI-thread only.
QHash<QQmlEngine *, std::shared_ptr<IconThemeLocator>> &engineLocators()
{
    static QHash<QQmlEngine *, std::shared_ptr<IconThemeLocator>> registry;
    return registry;
}

} // namespace

IconLookup *IconLookup::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine);
    if (engine == nullptr) {
        return new IconLookup(nullptr, nullptr);
    }
    return new IconLookup(engineLocators().value(engine), engine);
}

bool IconLookup::hasIcon(const QString &name, int size, double scale, bool symbolic) const
{
    if (m_locator == nullptr) {
        return false; // No IconRuntime::install on this engine: fail closed.
    }
    return m_locator->hasIcon(name, size, scale, symbolic);
}

void IconLookup::installForEngine(QQmlEngine *engine,
                                  std::shared_ptr<IconThemeLocator> locator)
{
    if (engine == nullptr) {
        return;
    }
    if (!engineLocators().contains(engine)) {
        QObject::connect(engine, &QObject::destroyed, engine, [engine] {
            engineLocators().remove(engine);
        });
    }
    engineLocators().insert(engine, std::move(locator));
}

IconLookup::IconLookup(std::shared_ptr<IconThemeLocator> locator, QQmlEngine *parent)
    : QObject(parent)
    , m_locator(std::move(locator))
{
}

} // namespace QindaQt::Shell::Icons
