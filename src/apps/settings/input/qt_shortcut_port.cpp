// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>
#include <optional>

namespace QindaQt::Apps::SettingsInput {
namespace {

Q_LOGGING_CATEGORY(lcShortcutPort, "qindaqt.settings.input.shortcuts",
                   QtInfoMsg)

constexpr auto Service = "org.kde.kglobalaccel";
constexpr auto ObjectPath = "/kglobalaccel";
constexpr auto Interface = "org.kde.KGlobalAccel";
constexpr auto ComponentInterface = "org.kde.kglobalaccel.Component";
constexpr int CallTimeoutMs = 4000;
constexpr qsizetype MaximumComponents = 256;
constexpr qsizetype MaximumActions = 4096;
constexpr qsizetype MaximumCommandLength = 512;
constexpr qsizetype MaximumNameLength = 128;
constexpr qint64 MaximumCommandFileBytes = 64 * 1024;
constexpr auto CommandDirectory = "kglobalaccel";

QDBusMessage callAuthority(const QDBusConnection &bus, const QString &path,
                           const QString &interface, const QString &method,
                           const QVariantList &arguments = {}) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(Service), path, interface, method);
    message.setArguments(arguments);
    return bus.call(message, QDBus::Block, CallTimeoutMs);
}

bool serviceAvailable(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(Service));
}

bool validIdentity(const QString &value) {
    return !value.isEmpty() && value.size() <= 256 &&
           !value.contains(QLatin1Char('/')) &&
           !value.contains(QLatin1Char('\n'));
}

// AGENT-GUARD: A reply is decoded only after its exact D-Bus signature is
// confirmed. QDBusArgument aborts the whole process (a libdbus check failure)
// when asked to read a type the message does not carry, so an unexpected
// authority reply must fail closed here instead of taking Settings down.
bool replyWithSignature(const QDBusMessage &reply, const char *signature) {
    return reply.type() == QDBusMessage::ReplyMessage &&
           reply.signature() == QLatin1String(signature) &&
           reply.arguments().size() == 1;
}

struct ComponentRow {
    QString unique;
    QString friendly;
};

struct InfoRow {
    QString actionUnique;
    QString actionFriendly;
    QString componentFriendly;
    QList<int> keys;
    QList<int> defaults;
};

std::optional<QList<ComponentRow>> mainComponents(const QDBusConnection &bus,
                                                  QString *error) {
    const QDBusMessage reply =
        callAuthority(bus, QLatin1String(ObjectPath), QLatin1String(Interface),
                      QStringLiteral("allMainComponents"));
    if (!replyWithSignature(reply, "aas")) {
        if (error != nullptr) {
            *error = QStringLiteral("allMainComponents reply is malformed");
        }
        return std::nullopt;
    }
    QList<ComponentRow> rows;
    const auto keep = [&rows](const QStringList &row) {
        if (rows.size() < MaximumComponents && validIdentity(row.value(0))) {
            rows.append({row.value(0), row.value(1)});
        }
    };
    const QVariant body = reply.arguments().at(0);
    if (body.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument argument = body.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QStringList row;
            argument >> row;
            keep(row);
        }
        argument.endArray();
    } else {
        for (const QVariant &row : body.toList()) {
            keep(row.toStringList());
        }
    }
    return rows;
}

std::optional<QList<InfoRow>> componentInfos(const QDBusConnection &bus,
                                             const QString &componentUnique) {
    const QDBusMessage pathReply = callAuthority(
        bus, QLatin1String(ObjectPath), QLatin1String(Interface),
        QStringLiteral("getComponent"), {componentUnique});
    if (!replyWithSignature(pathReply, "o")) {
        return std::nullopt;
    }
    const QString path =
        qvariant_cast<QDBusObjectPath>(pathReply.arguments().at(0)).path();
    if (path.isEmpty()) {
        return std::nullopt;
    }
    const QDBusMessage reply =
        callAuthority(bus, path, QLatin1String(ComponentInterface),
                      QStringLiteral("allShortcutInfos"));
    if (!replyWithSignature(reply, "a(ssssssaiai)") ||
        reply.arguments().at(0).userType() != qMetaTypeId<QDBusArgument>()) {
        return std::nullopt;
    }
    QList<InfoRow> rows;
    const QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        InfoRow row;
        QString reportedComponent;
        QString contextUnique;
        QString contextFriendly;
        argument.beginStructure();
        argument >> row.actionUnique >> row.actionFriendly >> reportedComponent >>
            row.componentFriendly >> contextUnique >> contextFriendly >>
            row.keys >> row.defaults;
        argument.endStructure();
        rows.append(row);
    }
    argument.endArray();
    return rows;
}

std::optional<QList<int>> activeChords(const QDBusConnection &bus,
                                       const QString &componentUnique,
                                       const QString &actionUnique) {
    const std::optional<QList<InfoRow>> rows =
        componentInfos(bus, componentUnique);
    if (!rows) {
        return std::nullopt;
    }
    for (const InfoRow &row : *rows) {
        if (row.actionUnique == actionUnique) {
            QList<int> chords;
            for (const int key : row.keys) {
                if (key != 0) {
                    chords.append(key);
                }
            }
            return chords;
        }
    }
    return std::nullopt;
}

// Reads Exec= out of one command component file this route created.
QString readCommand(const QString &dataHome, const QString &componentUnique) {
    QFile file(QDir(dataHome).absoluteFilePath(
        QStringLiteral("%1/%2").arg(QLatin1String(CommandDirectory),
                                    componentUnique)));
    if (!file.open(QIODevice::ReadOnly) ||
        file.size() > MaximumCommandFileBytes) {
        return {};
    }
    static const QRegularExpression execLine(
        QStringLiteral("^Exec=(.*)$"), QRegularExpression::MultilineOption);
    const auto match = execLine.match(
        QString::fromUtf8(file.read(MaximumCommandFileBytes)));
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument,
                          const ShortcutKeySequence &sequence) {
    argument.beginStructure();
    argument.beginArray(QMetaType::fromType<int>());
    for (const int chord : sequence.chords) {
        argument << chord;
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ShortcutKeySequence &sequence) {
    sequence = ShortcutKeySequence{};
    argument.beginStructure();
    argument.beginArray();
    std::size_t index = 0;
    while (!argument.atEnd()) {
        int chord = 0;
        argument >> chord;
        if (index < sequence.chords.size()) {
            sequence.chords[index] = chord;
        }
        ++index;
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

void registerShortcutDBusTypes() {
    static const bool registered = []() {
        qDBusRegisterMetaType<ShortcutKeySequence>();
        qDBusRegisterMetaType<QList<ShortcutKeySequence>>();
        return true;
    }();
    Q_UNUSED(registered);
}

QList<int> shortcutKeysToInts(const QList<QKeySequence> &sequences) {
    QList<int> keys;
    for (const QKeySequence &sequence : sequences) {
        if (sequence.count() < 1) {
            continue;
        }
        keys.append(int(sequence[0].toCombined()));
    }
    return keys;
}

QList<QKeySequence> shortcutKeysFromInts(const QList<int> &keys) {
    QList<QKeySequence> sequences;
    for (const int key : keys) {
        if (key == 0) {
            continue;
        }
        sequences.append(QKeySequence(QKeyCombination::fromCombined(key)));
    }
    return sequences;
}

QString keySequenceDisplay(const QKeySequence &sequence) {
    if (sequence.count() < 1) {
        return QString();
    }
    return sequence.toString(QKeySequence::NativeText);
}

ShortcutPort::~ShortcutPort() = default;

QtShortcutPort::QtShortcutPort(QDBusConnection bus, QString dataHome)
    : m_bus(std::move(bus)), m_dataHome(std::move(dataHome)) {
    registerShortcutDBusTypes();
}

QList<ShortcutAction> QtShortcutPort::actions(QString *error) const {
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.kde.kglobalaccel is not reachable");
        }
        return {};
    }
    const std::optional<QList<ComponentRow>> components =
        mainComponents(m_bus, error);
    if (!components) {
        return {};
    }
    QList<ShortcutAction> actions;
    for (const ComponentRow &component : *components) {
        const std::optional<QList<InfoRow>> rows =
            componentInfos(m_bus, component.unique);
        if (!rows) {
            continue; // the component vanished or answered malformed
        }
        for (const InfoRow &row : *rows) {
            if (!validIdentity(row.actionUnique) ||
                actions.size() >= MaximumActions) {
                continue;
            }
            ShortcutAction action;
            action.componentUnique = component.unique;
            action.componentFriendly = row.componentFriendly.isEmpty()
                                           ? component.friendly
                                           : row.componentFriendly;
            action.actionUnique = row.actionUnique;
            action.actionFriendly = row.actionFriendly;
            action.active = shortcutKeysFromInts(row.keys);
            action.defaults = shortcutKeysFromInts(row.defaults);
            if (action.isCommandComponent()) {
                action.command = readCommand(m_dataHome, component.unique);
            }
            actions.append(action);
        }
    }
    return actions;
}

bool QtShortcutPort::setShortcuts(const QString &componentUnique,
                                  const QString &actionUnique,
                                  const QList<QKeySequence> &keys,
                                  QString *error) const {
    if (!serviceAvailable(m_bus) || !validIdentity(componentUnique) ||
        !validIdentity(actionUnique)) {
        if (error != nullptr) {
            *error = QStringLiteral("Shortcut authority is not reachable or "
                                    "the action identity is invalid");
        }
        return false;
    }
    registerShortcutDBusTypes();
    QList<ShortcutKeySequence> wire;
    for (const QKeySequence &sequence : keys) {
        if (sequence.count() < 1) {
            continue;
        }
        ShortcutKeySequence entry;
        const int chords =
            std::min(sequence.count(), int(entry.chords.size()));
        for (int chord = 0; chord < chords; ++chord) {
            entry.chords[std::size_t(chord)] =
                sequence[uint(chord)].toCombined();
        }
        wire.append(entry);
    }
    // AGENT-CONTRACT: actionId is [component unique, action unique,
    // component friendly, action friendly]; kglobalaccel finds the action by
    // the first two. Only setForeignShortcutKeys with ShortcutKeySequence may
    // carry keys (never the integer setForeignShortcut), see the type's note.
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(Service), QLatin1String(ObjectPath),
        QLatin1String(Interface), QStringLiteral("setForeignShortcutKeys"));
    message.setArguments(
        {QVariant(QStringList{componentUnique, actionUnique, QString(), QString()}),
         QVariant::fromValue(wire)});
    const QDBusMessage reply = m_bus.call(message, QDBus::Block, CallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("setForeignShortcutKeys failed: %1")
                         .arg(reply.errorMessage());
        }
        return false;
    }
    // The authority answers success even when it ignored the action or kept
    // a key another action holds; only reading the keys back tells the truth.
    const std::optional<QList<int>> applied =
        activeChords(m_bus, componentUnique, actionUnique);
    if (!applied || *applied != shortcutKeysToInts(keys)) {
        if (error != nullptr) {
            *error = QStringLiteral("The desktop kept the previous shortcut "
                                    "for %1; another action may already use "
                                    "that key")
                         .arg(actionUnique);
        }
        return false;
    }
    return true;
}

bool QtShortcutPort::addCommandShortcut(const QString &name,
                                        const QString &command,
                                        const QList<QKeySequence> &keys,
                                        QString *componentUnique,
                                        QString *error) const {
    const QString trimmedName = name.trimmed();
    const QString trimmedCommand = command.trimmed();
    if (trimmedName.isEmpty() || trimmedName.size() > MaximumNameLength ||
        trimmedName.contains(QLatin1Char('\n')) || trimmedCommand.isEmpty() ||
        trimmedCommand.size() > MaximumCommandLength ||
        trimmedCommand.contains(QLatin1Char('\n'))) {
        if (error != nullptr) {
            *error = QStringLiteral("The command name and command line must "
                                    "be short, single-line, and non-empty");
        }
        return false;
    }
    QString storage;
    for (const QChar c : trimmedName) {
        if (c.isLetterOrNumber()) {
            storage.append(c.toLower());
        } else if (c == QLatin1Char(' ') || c == QLatin1Char('-') ||
                   c == QLatin1Char('_')) {
            storage.append(QLatin1Char('-'));
        }
    }
    while (storage.endsWith(QLatin1Char('-'))) {
        storage.chop(1);
    }
    if (storage.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The command name needs at least one "
                                    "letter or digit");
        }
        return false;
    }
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.kde.kglobalaccel is not reachable");
        }
        return false;
    }
    const QString id = QLatin1String(CommandComponentPrefix) + storage +
                       QStringLiteral(".desktop");
    QDir directory(
        QDir(m_dataHome).absoluteFilePath(QLatin1String(CommandDirectory)));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create %1")
                         .arg(directory.absolutePath());
        }
        return false;
    }
    const QString filePath = directory.absoluteFilePath(id);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error != nullptr) {
            *error = QFile::exists(filePath)
                         ? QStringLiteral("A command shortcut named %1 "
                                          "already exists")
                               .arg(trimmedName)
                         : QStringLiteral("Could not write %1").arg(filePath);
        }
        return false;
    }
    // AGENT-CONTRACT: kglobalaccel finds a desktop-file component under its
    // data directory and runs Exec when the `_launch` action triggers; the
    // CommandShortcut key marks it as a custom command the way the desktop's
    // own shortcut settings do. Launching was proven in a private KWin
    // (ADR-0134).
    const QByteArray contents =
        QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\n"
                       "NoDisplay=true\nX-KDE-GlobalAccel-CommandShortcut=true\n")
            .arg(trimmedName, trimmedCommand)
            .toUtf8();
    const bool wrote = file.write(contents) == contents.size();
    file.close();
    bool ok = wrote;
    if (ok) {
        const QDBusMessage registered = callAuthority(
            m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
            QStringLiteral("doRegister"),
            {QVariant(QStringList{id, QLatin1String(LaunchActionName),
                                  trimmedName, trimmedName})});
        ok = registered.type() == QDBusMessage::ReplyMessage;
        if (!ok && error != nullptr) {
            *error = QStringLiteral("doRegister failed: %1")
                         .arg(registered.errorMessage());
        }
    } else if (error != nullptr) {
        *error = QStringLiteral("Could not write %1").arg(filePath);
    }
    if (ok && !keys.isEmpty()) {
        ok = setShortcuts(id, QLatin1String(LaunchActionName), keys, error);
    }
    if (!ok) {
        // AGENT-GUARD: No registered-but-unassignable component and no orphan
        // file survive a failed add; listing and authority stay consistent.
        callAuthority(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
                      QStringLiteral("unregister"),
                      {id, QString::fromLatin1(LaunchActionName)});
        QFile::remove(filePath);
        qCInfo(lcShortcutPort, "removed command component %s after a failed add",
               qPrintable(id));
        return false;
    }
    if (componentUnique != nullptr) {
        *componentUnique = id;
    }
    return true;
}

bool QtShortcutPort::removeCommandShortcut(const QString &componentUnique,
                                           QString *error) const {
    ShortcutAction candidate;
    candidate.componentUnique = componentUnique;
    if (!validIdentity(componentUnique) || !candidate.isCommandComponent()) {
        if (error != nullptr) {
            *error = QStringLiteral("Only command shortcuts can be removed");
        }
        return false;
    }
    const QString filePath = QDir(m_dataHome).absoluteFilePath(
        QStringLiteral("%1/%2").arg(QLatin1String(CommandDirectory),
                                    componentUnique));
    if (!QFile::exists(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Command component %1 does not exist")
                         .arg(componentUnique);
        }
        return false;
    }
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.kde.kglobalaccel is not reachable");
        }
        return false;
    }
    const QDBusMessage reply = callAuthority(
        m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
        QStringLiteral("unregister"),
        {componentUnique, QString::fromLatin1(LaunchActionName)});
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("unregister failed: %1")
                         .arg(reply.errorMessage());
        }
        return false;
    }
    if (!QFile::remove(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not remove %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Apps::SettingsInput
