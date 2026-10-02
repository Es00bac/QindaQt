// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QKeySequence>
#include <QObject>
#include <QVariantMap>
namespace QindaQt::Services::Shortcuts {
inline constexpr auto Service = "org.qindaqt.Shortcuts1";
inline constexpr auto Path = "/org/qindaqt/Shortcuts1";
struct Binding { QString component, action, description, componentLabel; QList<QKeySequence> keys, defaults; bool active = false, repeat = false; };
// Public native wire adapter; same-thread borrowed bus. No fallback authority,
// UI or persistence. Bounded synchronous calls report exact refusal/conflict.
// Signals are tied to the current unique authority owner; reconnect emits lost.
class QtShortcutTransport final : public QObject {
    Q_OBJECT
public:
    explicit QtShortcutTransport(QDBusConnection bus, QObject *parent = nullptr);
    bool available() const;
    QList<Binding> bindings(QString *error = nullptr) const;
    bool registerBinding(const Binding &, bool transient, QString *error = nullptr) const;
    bool setShortcuts(const QString &component, const QString &action, const QList<QKeySequence> &, QString *error = nullptr) const;
    QStringList conflicts(const QList<QKeySequence> &, const QString &component, const QString &action, QString *error = nullptr) const;
    bool unregisterBinding(const QString &, const QString &, QString *error = nullptr) const;
Q_SIGNALS:
    void activated(const QString &, const QString &, quint64);
    void deactivated(const QString &, const QString &, quint64);
    void bindingsChanged();
    void authorityLost();
private Q_SLOTS:
    void ownerChanged(const QString &, const QString &, const QString &);
private:
    void attach(const QString &);
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watch;
    QString m_owner;
};
}
