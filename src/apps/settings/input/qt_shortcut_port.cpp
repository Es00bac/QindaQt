// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QMetaType>
#include <QRegularExpression>

#include <cstdio>

namespace QindaQt::Apps::SettingsInput {
namespace {

Q_LOGGING_CATEGORY(lcShortcutPort, "qindaqt.settings.input.shortcuts",
                   QtInfoMsg)

constexpr auto Service = "org.kde.kglobalaccel";
constexpr auto ObjectPath = "/kglobalaccel";
constexpr auto Interface = "org.kde.KGlobalAccel";
constexpr auto ComponentInterface = "org.kde.kglobalaccel.Component";
constexpr int CallTimeoutMs = 4000;
constexpr int MaximumComponents = 256;
constexpr int MaximumActionsPerComponent = 512;
constexpr qsizetype MaximumCommandLength = 512;
constexpr qsizetype MaximumNameLength = 128;
constexpr auto CommandDirectory = "kglobalaccel";
constexpr auto CommandPrefix = "qindaqt-custom-";

} // namespace

// Idempotent registration of the wire types with the D-Bus meta-type
// system (see the contract in the header). A static inside a function in
// the adapter's namespace keeps this a one-time, thread-safe init.
void registerShortcutDBusTypes() {
    static const bool registered = []() {
        // AGENT-NOTE: The name registrations are load-bearing: Qt D-Bus
        // resolves exported method parameters by type NAME at dispatch
        // time, and template type names are not in the metatype registry
        // unless qRegisterMetaType puts them there explicitly.
        qRegisterMetaType<ShortcutKeySequence>();
        qRegisterMetaType<QList<ShortcutKeySequence>>();
        qRegisterMetaType<ShortcutInfoRow>();
        qRegisterMetaType<QList<ShortcutInfoRow>>();
        qDBusRegisterMetaType<ShortcutKeySequence>();
        qDBusRegisterMetaType<QList<ShortcutKeySequence>>();
        qDBusRegisterMetaType<ShortcutInfoRow>();
        qDBusRegisterMetaType<QList<ShortcutInfoRow>>();
        return true;
    }();
    Q_UNUSED(registered);
}

namespace {

QDBusMessage call(const QDBusConnection &bus, const QString &path,
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



bool validComponentIdentity(const QString &componentUnique) {
    return !componentUnique.isEmpty() && componentUnique.size() <= 256 &&
           !componentUnique.contains(QLatin1Char('/')) &&
           !componentUnique.contains(QLatin1Char('\n'));
}

// Decodes a a(ai) reply argument: Qt hands it back either demarshalled to
// QList<int> or as a lazy QDBusArgument.
QList<int> keysFromReply(const QVariant &replyArgument) {
    if (replyArgument.userType() == QMetaType::QVariantList) {
        QList<int> keys;
        for (const QVariant &value : replyArgument.toList()) {
            keys.append(value.toInt());
        }
        return keys;
    }
    if (replyArgument.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument argument = replyArgument.value<QDBusArgument>();
        QList<int> keys;
        argument.beginArray();
        while (!argument.atEnd()) {
            int key = 0;
            argument >> key;
            keys.append(key);
        }
        argument.endArray();
        return keys;
    }
    return {};
}

// Reads Exec= out of one installed command component file.
QString readCommandComponent(const QString &dataHome,
                             const QString &componentUnique) {
    QFile file(QDir(dataHome).absoluteFilePath(
        QStringLiteral("%1/%2")
            .arg(QLatin1String(CommandDirectory), componentUnique)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QRegularExpression execLine(QStringLiteral("^Exec=(.*)$"),
                                      QRegularExpression::MultilineOption);
    const auto match = execLine.match(QString::fromUtf8(file.readAll()));
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

} // namespace

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
    registerShortcutDBusTypes();
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.kde.kglobalaccel is not reachable");
        }
        return {};
    }
    const QDBusMessage componentsReply =
        call(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
             QStringLiteral("allMainComponents"));
    if (componentsReply.type() != QDBusMessage::ReplyMessage ||
        componentsReply.arguments().size() != 1) {
        if (error != nullptr) {
            *error = QStringLiteral("allMainComponents failed: %1")
                         .arg(componentsReply.errorMessage());
        }
        return {};
    }
    // Decode the component rows. The daemon replies aas; Qt may hand the
    // reply back as a lazy QDBusArgument or as a demarshalled list.
    QList<QStringList> rowsDecoded;
    const QVariant rowsVariant = componentsReply.arguments().at(0);
    bool decoded = false;
    if (rowsVariant.userType() == qMetaTypeId<QDBusArgument>()) {
        decoded = true;
        const QDBusArgument argument = rowsVariant.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QStringList row;
            argument.beginArray();
            while (!argument.atEnd()) {
                QString value;
                argument >> value;
                row.append(value);
            }
            argument.endArray();
            // AGENT-GUARD: Daemon rows always carry at least the component
            // identity; an empty inner array means the reply shape is not
            // what the route understands. Fail closed.
            if (row.isEmpty()) {
                if (error != nullptr) {
                    *error = QStringLiteral(
                        "allMainComponents reply is malformed");
                }
                return {};
            }
            rowsDecoded.append(row);
        }
        argument.endArray();
    } else if (rowsVariant.canConvert<QVariantList>()) {
        decoded = true;
        for (const QVariant &row : rowsVariant.toList()) {
            QStringList rowStrings;
            for (const QVariant &value : row.toList()) {
                rowStrings.append(value.toString());
            }
            // AGENT-GUARD: Daemon rows always carry the component identity;
            // a row that decodes to nothing is an uninterpretable reply.
            // Fail closed instead of presenting a partial listing.
            if (rowStrings.isEmpty()) {
                if (error != nullptr) {
                    *error = QStringLiteral(
                        "allMainComponents reply is malformed");
                }
                return {};
            }
            rowsDecoded.append(rowStrings);
        }
    }
    if (!decoded) {
        // AGENT-GUARD: A reply body the route cannot interpret as a
        // component list is an authority failure, not an empty listing.
        if (error != nullptr) {
            *error = QStringLiteral("allMainComponents reply is malformed");
        }
        return {};
    }

    QList<ShortcutAction> actions;
    int componentsVisited = 0;
    for (const QStringList &rowStrings : std::as_const(rowsDecoded)) {
        const QString componentUnique = rowStrings.value(0);
        const QString componentFriendly = rowStrings.value(1);
        if (!validComponentIdentity(componentUnique)) {
            continue;
        }
        // AGENT-GUARD: Bound the enumeration so a runaway authority reply
        // cannot hang the Settings window.
        if (componentsVisited >= MaximumComponents) {
            break;
        }
        ++componentsVisited;
        const QDBusMessage actionsReply =
            call(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
                 QStringLiteral("allActionsForComponent"),
                 {QVariant(QStringList{componentUnique})});
        if (actionsReply.type() != QDBusMessage::ReplyMessage ||
            actionsReply.arguments().size() != 1) {
            continue; // component vanished mid-listing
        }
        QList<QStringList> actionRows;
        const QVariant actionsVariant = actionsReply.arguments().at(0);
        if (actionsVariant.userType() == qMetaTypeId<QDBusArgument>()) {
            const QDBusArgument argument =
                actionsVariant.value<QDBusArgument>();
            argument.beginArray();
            while (!argument.atEnd()) {
                QStringList row;
                argument.beginArray();
                while (!argument.atEnd()) {
                    QString value;
                    argument >> value;
                    row.append(value);
                }
                argument.endArray();
                actionRows.append(row);
            }
            argument.endArray();
        } else if (actionsVariant.canConvert<QVariantList>()) {
            for (const QVariant &row : actionsVariant.toList()) {
                QStringList cells;
                for (const QVariant &value : row.toList()) {
                    cells.append(value.toString());
                }
                actionRows.append(cells);
            }
        }
        for (const QStringList &row : std::as_const(actionRows)) {
            if (row.value(1).isEmpty() ||
                actions.size() >= MaximumActionsPerComponent) {
                continue;
            }
            ShortcutAction action;
            action.componentUnique = componentUnique;
            action.componentFriendly = componentFriendly;
            action.actionUnique = row.value(1);
            action.actionFriendly = row.value(3);
            action.active = shortcutKeys(componentUnique, action.actionUnique);
            action.defaults =
                defaultShortcutKeys(componentUnique, action.actionUnique);
            if (action.isCommandComponent()) {
                action.command = readCommandComponent(m_dataHome, action.componentUnique);
            }
            actions.append(action);
        }
    }
    return actions;
}

// Built-in-type accessors for one action's active and default chords; both
// methods live on /kglobalaccel with a(ai) replies (live-verified).
QList<QKeySequence> QtShortcutPort::shortcutKeys(
    const QString &componentUnique, const QString &actionUnique) const {
    const QDBusMessage reply =
        call(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
             QStringLiteral("shortcutKeys"),
             {QVariant(QStringList{componentUnique, actionUnique})});
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().size() != 1) {
        return {};
    }
    return shortcutKeysFromInts(keysFromReply(reply.arguments().at(0)));
}

QList<QKeySequence> QtShortcutPort::defaultShortcutKeys(
    const QString &componentUnique, const QString &actionUnique) const {
    const QDBusMessage reply =
        call(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
             QStringLiteral("defaultShortcutKeys"),
             {QVariant(QStringList{componentUnique, actionUnique})});
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().size() != 1) {
        return {};
    }
    return shortcutKeysFromInts(keysFromReply(reply.arguments().at(0)));
}

bool QtShortcutPort::setShortcuts(const QString &componentUnique,
                                  const QString &actionUnique,
                                  const QList<QKeySequence> &keys,
                                  QString *error) const {
    if (!serviceAvailable(m_bus) || !validComponentIdentity(componentUnique) ||
        actionUnique.isEmpty() || actionUnique.size() > 256) {
        if (error != nullptr) {
            *error = QStringLiteral("Shortcut authority is not reachable or "
                                    "the action identity is invalid");
        }
        return false;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(Service), QLatin1String(ObjectPath),
        QLatin1String(Interface), QStringLiteral("setForeignShortcut"));
    // Positional contract of setForeignShortcut(asai): one string list
    // holding the action identity (component unique, component friendly,
    // action unique, action friendly, context unique, context friendly)
    // and one flat int array of key chords. The daemon keys on the unique
    // names; built-in types keep the dispatch metatype-free.
    const QStringList identity = {
        componentUnique, componentUnique, actionUnique, actionUnique,
        QStringLiteral("default"), QStringLiteral("Default Context")};
    const QVariantList messageArguments{
        QVariant::fromValue(identity),
        QVariant::fromValue(shortcutKeysToInts(keys))};
    message.setArguments(messageArguments);
    const QDBusMessage reply =
        m_bus.call(message, QDBus::Block, CallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("setForeignShortcutKeys failed: %1")
                         .arg(reply.errorMessage());
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
        trimmedCommand.isEmpty() ||
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
    while (storage.size() > 1 && storage.endsWith(QLatin1Char('-'))) {
        storage.chop(1);
    }
    if (storage.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The command name needs at least one "
                                    "letter or digit");
        }
        return false;
    }
    // AGENT-CONTRACT: The .desktop file name IS the kglobalaccel component
    // id (live desktop-backed components list as "<storage>.desktop").
    // Writing only under the injected data home keeps tests off the real
    // user's shortcut registry (never ~/.local/share/kglobalaccel).
    const QString id =
        CommandPrefix + storage + QStringLiteral(".desktop");
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1").arg(filePath);
        }
        return false;
    }
    const QString contents =
        QStringLiteral("[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=%1\n"
                       "Exec=%2\n"
                       "NoDisplay=true\n")
            .arg(trimmedName, trimmedCommand);
    const bool wrote = file.write(contents.toUtf8()) >= 0;
    file.close();
    if (!wrote) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1").arg(filePath);
        }
        return false;
    }
    if (!setShortcuts(id, id, keys, error)) {
        // AGENT-GUARD: Do not leave a registered-but-unassignable file
        // behind: a failed shortcut assignment removes the component again
        // so the route's listing and the authority stay consistent.
        qCInfo(lcShortcutPort,
               "removing command component file after a failed shortcut "
               "assignment: %s",
               qPrintable(filePath));
        QFile::remove(filePath);
        return false;
    }
    if (componentUnique != nullptr) {
        *componentUnique = id;
    }
    return true;
}

bool QtShortcutPort::removeCommandShortcut(const QString &componentUnique,
                                           QString *error) const {
    if (!validComponentIdentity(componentUnique) ||
        !componentUnique.endsWith(QStringLiteral(".desktop"))) {
        if (error != nullptr) {
            *error = QStringLiteral("Only command shortcuts can be removed");
        }
        return false;
    }
    const QString filePath =
        QDir(m_dataHome)
            .absoluteFilePath(QStringLiteral("%1/%2")
                                  .arg(QLatin1String(CommandDirectory),
                                       componentUnique));
    if (!QFile::exists(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Command component %1 does not exist")
                         .arg(componentUnique);
        }
        return false;
    }
    call(m_bus, QLatin1String(ObjectPath), QLatin1String(Interface),
         QStringLiteral("unregister"), {componentUnique, componentUnique});
    if (!QFile::remove(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not remove %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Apps::SettingsInput
