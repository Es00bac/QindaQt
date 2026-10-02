// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/services/shortcuts_client/transport.h>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <algorithm>
namespace QindaQt::Apps::SettingsInput {
namespace {
QString readCommand(const QString &home, const QString &component) {
    if (!component.startsWith(QLatin1String(CommandComponentPrefix)) || component.contains('/')) return {};
    QFile file(QDir(home).filePath(QStringLiteral("kglobalaccel/") + component));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return {};
    for (const auto &line : file.readAll().split('\n')) if (line.startsWith("Exec=")) return QString::fromUtf8(line.mid(5));
    return {};
}
}
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

QtShortcutPort::QtShortcutPort(QDBusConnection bus, QString home) : m_bus(bus), m_dataHome(std::move(home)) {}
QList<ShortcutAction> QtShortcutPort::actions(QString *error) const {
    const auto bindings = QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).bindings(error);
    QList<ShortcutAction> result;
    for (const auto &binding : bindings) {
        ShortcutAction action; action.componentUnique = binding.component; action.componentFriendly = binding.componentLabel;
        action.actionUnique = binding.action; action.actionFriendly = binding.description;
        action.active = binding.keys; action.defaults = binding.defaults; action.command = readCommand(m_dataHome, binding.component);
        result.append(action);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.componentUnique == b.componentUnique ? a.actionUnique < b.actionUnique : a.componentUnique < b.componentUnique; });
    return result;
}
bool QtShortcutPort::setShortcuts(const QString &component, const QString &action, const QList<QKeySequence> &keys, QString *error) const {
    return QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).setShortcuts(component, action, keys, error);
}

} // namespace QindaQt::Apps::SettingsInput
