// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QQmlApplicationEngine>

namespace QindaQt::Apps::SettingsCenter {

// AGENT-GUARD (ADR-0395): destroys the QML scene while the objects it reads are
// still alive.
//
// Settings hands its models to QML with QQmlApplicationEngine::
// setInitialProperties, so the scene holds bindings, Connections and
// destruction handlers that read them. The engine is created before the
// models (the models need it: the token facade and the icon runtime live in
// it), which makes plain scope exit destroy the models first and the QML root
// last. Every binding and Component.onDestruction handler then runs against an
// object that no longer exists, which is how Settings aborted ten times between
// 2026-10-08 and 2026-10-16.
//
// Declare ONE of these in the composition root AFTER the last model. Reverse
// declaration order then destroys it first, on every exit path (a plain
// return, the route-construction probe, an exception), and it deletes the
// engine's root objects and collects the script heap before any model goes. The
// engine itself outlives the models, which is safe: it holds no model by
// ownership, only QObject pointers that QML wrappers already clear when the
// object is destroyed.
class QmlTeardown final {
public:
  explicit QmlTeardown(QQmlApplicationEngine &engine) noexcept : m_engine(engine) {}
  QmlTeardown(const QmlTeardown &) = delete;
  QmlTeardown &operator=(const QmlTeardown &) = delete;
  ~QmlTeardown() {
    // rootObjects() holds QPointers, so deleting one here is safe even if the
    // application already deleted another.
    const QList<QObject *> roots = m_engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
    m_engine.collectGarbage();
  }

private:
  QQmlApplicationEngine &m_engine;
};

} // namespace QindaQt::Apps::SettingsCenter
