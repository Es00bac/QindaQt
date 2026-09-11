// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

#include <qindaqt/apps/settings_input/shortcut_port.h>

namespace QindaQt::Tests
{

// One allShortcutInfos row in the daemon's wire shape (ssssssaiai).
struct FakeShortcutInfoWire {
    QString actionUnique;
    QString actionFriendly;
    QString componentUnique;
    QString componentFriendly;
    QString contextUnique;
    QString contextFriendly;
    QList<int> keys;
    QList<int> defaults;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const FakeShortcutInfoWire &row)
{
    argument.beginStructure();
    argument << row.actionUnique << row.actionFriendly << row.componentUnique
             << row.componentFriendly << row.contextUnique << row.contextFriendly
             << row.keys << row.defaults;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, FakeShortcutInfoWire &row)
{
    argument.beginStructure();
    argument >> row.actionUnique >> row.actionFriendly >> row.componentUnique
             >> row.componentFriendly >> row.contextUnique >> row.contextFriendly
             >> row.keys >> row.defaults;
    argument.endStructure();
    return argument;
}

// Fake org.kde.kglobalaccel reproducing the daemon's wire contract as observed
// in a private KWin (ADR-0134): allMainComponents (aas), getComponent (o), the
// component object's allShortcutInfos (a(ssssssaiai)), setForeignShortcutKeys
// (asa(ai), four ints per sequence), doRegister (as), unregister (ss). Unknown
// actions are ignored and a key another action holds is dropped, silently,
// exactly like the daemon.
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
        qDBusRegisterMetaType<FakeShortcutInfoWire>();
        qDBusRegisterMetaType<QList<FakeShortcutInfoWire>>();
        return bus.registerService(QStringLiteral("org.kde.kglobalaccel"))
            && bus.registerVirtualObject(QStringLiteral("/"), this, QDBusConnection::SubPath);
    }

    void addComponent(const QString &unique, const QString &friendly)
    {
        m_componentFriendly.insert(unique, friendly);
    }

    void addAction(const QString &componentUnique, const QString &actionUnique,
                   const QString &actionFriendly, const QList<int> &active,
                   const QList<int> &defaults)
    {
        m_actions.append({componentUnique, actionUnique, actionFriendly, active, defaults});
    }

    bool hasAction(const QString &componentUnique, const QString &actionUnique) const
    {
        return std::any_of(m_actions.cbegin(), m_actions.cend(), [&](const Action &candidate) {
            return candidate.componentUnique == componentUnique
                && candidate.actionUnique == actionUnique;
        });
    }

    Action action(const QString &componentUnique, const QString &actionUnique) const
    {
        for (const Action &candidate : m_actions) {
            if (candidate.componentUnique == componentUnique
                && candidate.actionUnique == actionUnique) {
                return candidate;
            }
        }
        return {};
    }

    // Negative-control switches and observations.
    bool malformedComponentList = false;
    bool malformedShortcutInfos = false;
    int legacyIntegerCalls = 0;
    int malformedSequenceCalls = 0;
    QStringList callOrder;
    QStringList lastActionId;
    QList<QList<int>> lastSequences;
    QList<QStringList> unregistered;

    QString introspect(const QString &) const override
    {
        return QStringLiteral("<node/>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        callOrder.append(message.member());
        if (message.path().startsWith(QLatin1String("/component/"))) {
            return handleComponent(message, connection);
        }
        if (message.interface() != QLatin1String("org.kde.KGlobalAccel")) {
            return false;
        }
        const QString member = message.member();
        if (member == QLatin1String("allMainComponents")) {
            if (malformedComponentList) {
                connection.send(message.createReply(
                    QVariant(QStringList{QStringLiteral("not"), QStringLiteral("rows")})));
                return true;
            }
            QDBusArgument rows;
            rows.beginArray(QMetaType::fromType<QStringList>());
            for (auto it = m_componentFriendly.cbegin(); it != m_componentFriendly.cend(); ++it) {
                rows << QStringList{it.key(), it.value(), QString(), QString()};
            }
            rows.endArray();
            connection.send(message.createReply(QVariant::fromValue(rows)));
            return true;
        }
        if (member == QLatin1String("getComponent") && message.signature() == QLatin1String("s")) {
            const QString unique = message.arguments().at(0).toString();
            if (!m_componentFriendly.contains(unique)) {
                connection.send(message.createErrorReply(
                    QStringLiteral("org.kde.kglobalaccel.NoSuchComponent"), unique));
                return true;
            }
            connection.send(message.createReply(QVariant::fromValue(
                QDBusObjectPath(QStringLiteral("/component/") + escaped(unique)))));
            return true;
        }
        if (member == QLatin1String("doRegister") && message.signature() == QLatin1String("as")) {
            const QStringList id = message.arguments().at(0).toStringList();
            if (id.size() >= 4 && !hasAction(id.at(0), id.at(1))) {
                if (!m_componentFriendly.contains(id.at(0))) {
                    m_componentFriendly.insert(id.at(0), id.at(2));
                }
                m_actions.append({id.at(0), id.at(1), id.at(3), {}, {}});
            }
            connection.send(message.createReply());
            return true;
        }
        if (member == QLatin1String("setForeignShortcutKeys")
            && message.signature() == QLatin1String("asa(ai)")) {
            return handleSetForeignShortcutKeys(message, connection);
        }
        if (member == QLatin1String("setForeignShortcut")) {
            ++legacyIntegerCalls;
            connection.send(message.createErrorReply(QStringLiteral("org.qindaqt.Test.LegacyCall"),
                                                     QStringLiteral("use setForeignShortcutKeys")));
            return true;
        }
        if (member == QLatin1String("unregister") && message.signature() == QLatin1String("ss")) {
            const QString component = message.arguments().at(0).toString();
            const QString actionUnique = message.arguments().at(1).toString();
            unregistered.append({component, actionUnique});
            const qsizetype removed = m_actions.removeIf([&](const Action &candidate) {
                return candidate.componentUnique == component
                    && candidate.actionUnique == actionUnique;
            });
            connection.send(message.createReply(QVariant(removed > 0)));
            return true;
        }
        return false; // unknown call: Qt produces the error reply
    }

private:
    bool handleComponent(const QDBusMessage &message, const QDBusConnection &connection)
    {
        if (message.member() != QLatin1String("allShortcutInfos") || !message.signature().isEmpty()) {
            return false;
        }
        if (malformedShortcutInfos) {
            connection.send(message.createReply(QVariant::fromValue(QList<int>{1, 2, 3})));
            return true;
        }
        const QString component = componentForPath(message.path());
        QDBusArgument rows;
        rows.beginArray(QMetaType::fromType<FakeShortcutInfoWire>());
        for (const Action &candidate : std::as_const(m_actions)) {
            if (candidate.componentUnique != component) {
                continue;
            }
            rows << FakeShortcutInfoWire{candidate.actionUnique, candidate.actionFriendly,
                                         candidate.componentUnique,
                                         m_componentFriendly.value(candidate.componentUnique),
                                         QStringLiteral("default"),
                                         QStringLiteral("Default Context"),
                                         candidate.active, candidate.defaults};
        }
        rows.endArray();
        connection.send(message.createReply(QVariant::fromValue(rows)));
        return true;
    }

    bool handleSetForeignShortcutKeys(const QDBusMessage &message, const QDBusConnection &connection)
    {
        lastActionId = message.arguments().at(0).toStringList();
        lastSequences.clear();
        bool wellFormed = true;
        const QDBusArgument sequences = message.arguments().at(1).value<QDBusArgument>();
        sequences.beginArray();
        while (!sequences.atEnd()) {
            QList<int> chords;
            sequences.beginStructure();
            sequences >> chords;
            sequences.endStructure();
            wellFormed = wellFormed && chords.size() == 4;
            lastSequences.append(chords);
        }
        sequences.endArray();
        // AGENT-CONTRACT: KF6GlobalAccel reads four ints per sequence and aborts
        // the hosting compositor otherwise; the fake refuses such a call loudly
        // so a port regression fails its row instead of passing.
        if (!wellFormed) {
            ++malformedSequenceCalls;
            connection.send(message.createErrorReply(
                QStringLiteral("org.qindaqt.Test.MalformedSequence"),
                QStringLiteral("each key sequence must carry four ints")));
            return true;
        }
        applyKeys(lastActionId.value(0), lastActionId.value(1));
        connection.send(message.createReply());
        return true;
    }

    void applyKeys(const QString &componentUnique, const QString &actionUnique)
    {
        Action *target = nullptr;
        for (Action &candidate : m_actions) {
            if (candidate.componentUnique == componentUnique
                && candidate.actionUnique == actionUnique) {
                target = &candidate;
            }
        }
        if (target == nullptr) {
            return; // the daemon ignores unknown actions
        }
        QList<int> accepted;
        for (const QList<int> &chords : std::as_const(lastSequences)) {
            const int chord = chords.value(0);
            const bool heldElsewhere = std::any_of(
                m_actions.cbegin(), m_actions.cend(), [&](const Action &other) {
                    return &other != target && other.active.contains(chord);
                });
            if (chord != 0 && !heldElsewhere) {
                accepted.append(chord);
            }
        }
        target->active = accepted;
    }

    static QString escaped(const QString &unique)
    {
        static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9_]"));
        QString path = unique;
        path.replace(unsafe, QStringLiteral("_"));
        return path;
    }

    QString componentForPath(const QString &path) const
    {
        const QString tail = path.mid(QStringLiteral("/component/").size());
        for (auto it = m_componentFriendly.cbegin(); it != m_componentFriendly.cend(); ++it) {
            if (escaped(it.key()) == tail) {
                return it.key();
            }
        }
        return {};
    }

    QMap<QString, QString> m_componentFriendly;
    QList<Action> m_actions;
};

} // namespace QindaQt::Tests
