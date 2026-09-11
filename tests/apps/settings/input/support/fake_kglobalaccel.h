// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>

#include <cstdio>
#include <QDBusMetaType>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <qindaqt/apps/settings_input/shortcut_port.h>

namespace QindaQt::Tests
{

// Fake kglobalaccel authority, service org.kde.kglobalaccel, implemented as
// a QDBusVirtualObject: handleMethodCall receives every raw method call, so
// the fake reproduces the daemon's exact wire behavior (a(ai) key encoding
// and a(ssssssaiai) info rows) without depending on Qt's metatype-based
// slot dispatch, which cannot resolve custom-typed IN parameters for plain
// exported objects (ADR-0134 notes).
class FakeKGlobalAccel final : public QDBusVirtualObject
{
public:
    struct Action {
        QString componentUnique;
        QString actionUnique;
        QString actionFriendly;
        QList<int> active;
        QList<int> defaults;
    };

    bool publish(QDBusConnection bus)
    {
        QindaQt::Apps::SettingsInput::registerShortcutDBusTypes();
        {
            const QMetaType element =
                QMetaType::fromType<QindaQt::Apps::SettingsInput::
                                        ShortcutKeySequence>();
            const QMetaType list =
                QMetaType::fromType<QList<QindaQt::Apps::SettingsInput::
                                              ShortcutKeySequence>>();
            std::fprintf(stderr,
                         "ELEM-SIG: name=%s dbus=%s | LIST dbus=%s\n",
                         element.name(),
                         QDBusMetaType::typeToSignature(element)
                             ? QDBusMetaType::typeToSignature(element)
                             : "NULL",
                         QDBusMetaType::typeToSignature(list)
                             ? QDBusMetaType::typeToSignature(list)
                             : "NULL");
        }
        m_bus = bus;
        // SubPath: one virtual object handles every path, matching the
        // daemon's /kglobalaccel and /component/<id> objects.
        return bus.registerService(QStringLiteral("org.kde.kglobalaccel")) &&
               bus.registerVirtualObject(QStringLiteral("/"), this,
                                         QDBusConnection::SubPath);
    }

    void addComponent(const QString &unique, const QString &friendly)
    {
        m_componentFriendly.insert(unique, friendly);
    }

    void addAction(const QString &componentUnique, const QString &actionUnique,
                   const QString &actionFriendly, const QList<int> &active,
                   const QList<int> &defaults)
    {
        m_actions.append({componentUnique, actionUnique, actionFriendly,
                          active, defaults});
    }

    // Negative-control switches.
    bool malformedComponentList = false;
    int setForeignCalls = 0;

    const QList<Action> &actionList() const { return m_actions; }

    // QDBusVirtualObject
    QString introspect(const QString &) const override
    {
        // Minimal but valid: the ports never introspect.
        return QStringLiteral("<node/>");
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        Q_UNUSED(connection);
        std::fprintf(stderr, "VCALL: path=%s member=%s iface=%s sig=%s\n",
                     qPrintable(message.path()),
                     qPrintable(message.member()),
                     qPrintable(message.interface()),
                     qPrintable(message.signature()));
        // Component objects: one allShortcutInfos per component id.
        if (message.path().startsWith(QLatin1String("/component/")) &&
            message.member() == QLatin1String("allShortcutInfos")) {
            QString unique = message.path();
            unique.remove(0, QLatin1String("/component/").size());
            unique.replace(QLatin1Char('_'), QLatin1Char('.'));
            // The path escaping is lossy ('-' also became '_'); match
            // against the escaped forms of the known component ids.
            QString canonical;
            for (auto it = m_componentFriendly.cbegin();
                 it != m_componentFriendly.cend(); ++it) {
                QString escaped = it.key();
                escaped.replace(QLatin1Char('.'), QLatin1Char('_'));
                escaped.replace(QLatin1Char('-'), QLatin1Char('_'));
                if (escaped == unique) {
                    canonical = it.key();
                    break;
                }
            }
            using QindaQt::Apps::SettingsInput::ShortcutInfoRow;
            QDBusArgument array;
            array.beginArray(
                QMetaType::fromType<ShortcutInfoRow>());
            for (const ShortcutInfoRow &row :
                 allShortcutInfosFor(canonical)) {
                array << row;
            }
            array.endArray();
            QDBusMessage reply = message.createReply(
                QVariant::fromValue(array));
            m_bus.send(reply);
            return true;
        }
        if (message.member() == QLatin1String("allActionsForComponent") &&
            message.interface() == QLatin1String("org.kde.KGlobalAccel") &&
            message.signature() == QLatin1String("as")) {
            const QString componentUnique =
                message.arguments().at(0).toStringList().value(0);
            QDBusArgument array;
            array.beginArray(QMetaType::QStringList);
            for (const Action &action : std::as_const(m_actions)) {
                if (action.componentUnique != componentUnique) {
                    continue;
                }
                array << QStringList{componentUnique, action.actionUnique,
                                     m_componentFriendly.value(componentUnique),
                                     action.actionFriendly};
            }
            array.endArray();
            QDBusMessage reply =
                message.createReply(QVariant::fromValue(array));
            m_bus.send(reply);
            return true;
        }
        if ((message.member() == QLatin1String("shortcutKeys") ||
             message.member() == QLatin1String("defaultShortcutKeys")) &&
            message.interface() == QLatin1String("org.kde.KGlobalAccel") &&
            message.signature() == QLatin1String("as")) {
            // as = (component unique, action unique)
            const QStringList identity =
                message.arguments().at(0).toStringList();
            QList<int> keys;
            for (const Action &action : std::as_const(m_actions)) {
                if (action.componentUnique == identity.value(0) &&
                    action.actionUnique == identity.value(1)) {
                    keys = message.member() == QLatin1String("shortcutKeys")
                               ? action.active
                               : action.defaults;
                }
            }
            QDBusMessage reply = message.createReply(QVariant::fromValue(keys));
            m_bus.send(reply);
            return true;
        }
        if (message.member() == QLatin1String("allMainComponents") &&
            message.interface() == QLatin1String("org.kde.KGlobalAccel")) {
            if (malformedComponentList) {
                // A reply with no body: uninterpretable for this protocol.
                QDBusMessage reply = message.createReply();
                m_bus.send(reply);
                return true;
            }
            // aas: one array of strings per component row, marshalled
            // explicitly so the reply signature matches the daemon.
            QDBusArgument array;
            array.beginArray(QMetaType::QStringList);
            for (auto it = m_componentFriendly.cbegin();
                 it != m_componentFriendly.cend(); ++it) {
                array << QStringList{it.key(), it.value(), QString(),
                                     QString()};
            }
            array.endArray();
            QDBusMessage reply = message.createReply(
                QVariant::fromValue(array));
            m_bus.send(reply);
            return true;
        }
        if (message.member() == QLatin1String("setForeignShortcut") &&
            message.interface() == QLatin1String("org.kde.KGlobalAccel") &&
            message.signature() == QLatin1String("asai")) {
            // AGENT-CONTRACT: setForeignShortcut(asai) — one string list
            // (component unique, component friendly, action unique, action
            // friendly, context unique, context friendly) and one flat key
            // array; built-in types, live-verified in ADR-0134.
            const QList<QVariant> arguments = message.arguments();
            const QStringList identity = arguments.at(0).toStringList();
            const QString actionUnique = identity.value(2);
            QList<int> flat;
            {
                const QVariant keysVariant = arguments.at(1);
                QList<int> keys;
                if (keysVariant.typeId() == QMetaType::QVariantList) {
                    for (const QVariant &key : keysVariant.toList()) {
                        keys.append(key.toInt());
                    }
                } else {
                    const QDBusArgument argument =
                        keysVariant.value<QDBusArgument>();
                    argument.beginArray();
                    while (!argument.atEnd()) {
                        int key = 0;
                        argument >> key;
                        keys.append(key);
                    }
                    argument.endArray();
                }
                flat = keys;
            }
            applySetForeignShortcutKeys(identity.value(0), actionUnique,
                                        flat);
            QDBusMessage reply = message.createReply();
            m_bus.send(reply);
            return true;
        }
        return false; // unknown call: Qt produces the error reply
    }

    // Test observation.
    QList<QindaQt::Apps::SettingsInput::ShortcutInfoRow>
    allShortcutInfosFor(const QString &componentUnique) const
    {
        QList<QindaQt::Apps::SettingsInput::ShortcutInfoRow> rows;
        for (const Action &action : std::as_const(m_actions)) {
            if (action.componentUnique != componentUnique) {
                continue;
            }
            QindaQt::Apps::SettingsInput::ShortcutInfoRow row;
            row.actionUnique = action.actionUnique;
            row.actionFriendly = action.actionFriendly;
            row.componentUnique = action.componentUnique;
            row.componentFriendly =
                m_componentFriendly.value(action.componentUnique,
                                          action.componentUnique);
            row.contextUnique = QStringLiteral("default");
            row.contextFriendly = QStringLiteral("Default Context");
            row.keys.keys = action.active;
            row.defaults.keys = action.defaults;
            rows.append(row);
        }
        return rows;
    }

private:
    void applySetForeignShortcutKeys(const QString &componentUnique,
                                     const QString &actionUnique,
                                     const QList<int> &flat)
    {
        ++setForeignCalls;
        for (Action &action : m_actions) {
            if (action.componentUnique == componentUnique &&
                action.actionUnique == actionUnique) {
                action.active = flat;
                return;
            }
        }
        // The daemon creates unknown components on foreign assignment.
        m_actions.append({componentUnique, actionUnique, actionUnique, flat,
                          {}});
    }

    QVariantList allMainComponents() const
    {
        QVariantList rows;
        for (auto it = m_componentFriendly.cbegin();
             it != m_componentFriendly.cend(); ++it) {
            rows.append(QVariantList{it.key(), it.value(), QString(),
                                     QString()});
        }
        return rows;
    }

    QDBusConnection m_bus{QStringLiteral("none")};
    QMap<QString, QString> m_componentFriendly;
    QList<Action> m_actions;
};

} // namespace QindaQt::Tests
