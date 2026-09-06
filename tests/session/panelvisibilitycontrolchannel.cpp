// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitycontrolchannel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

#include <cmath>

namespace QindaQt::Test::PanelVisibilityControl {
namespace {

constexpr int SchemaVersion = 1;

bool readPositiveSequence(const QJsonObject &object, quint64 *sequence)
{
    const QJsonValue value = object.value(QStringLiteral("sequence"));
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (number <= 0.0 || number > 9'007'199'254'740'991.0
        || std::floor(number) != number) {
        return false;
    }
    *sequence = static_cast<quint64>(number);
    return true;
}

} // namespace

Channel::Channel(QString commandPath)
    : m_commandPath(std::move(commandPath))
    , m_ackPath(m_commandPath + QStringLiteral(".ack"))
{
}

bool Channel::isValid(QString *error) const
{
    const QFileInfo info(m_commandPath);
    if (!info.isAbsolute()) {
        *error = QStringLiteral("control file path is not absolute");
        return false;
    }
    if (!QFileInfo(info.absolutePath()).isDir()) {
        *error = QStringLiteral("control file parent directory is unavailable");
        return false;
    }
    return true;
}

bool Channel::publishReady(const QString &title, QString *error) const
{
    return writeAcknowledgement(
        QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), SchemaVersion},
                                  {QStringLiteral("status"), QStringLiteral("ready")},
                                  {QStringLiteral("title"), title}})
            .toJson(QJsonDocument::Compact),
        error);
}

std::optional<Command> Channel::readNext(QString *error)
{
    QFile commandFile(m_commandPath);
    if (!commandFile.exists()) {
        return std::nullopt;
    }
    if (!commandFile.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read control file");
        return std::nullopt;
    }
    const QByteArray bytes = commandFile.readAll();
    if (bytes.isEmpty() || bytes == m_lastCommand) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) {
        *error = QStringLiteral("control command is not a JSON object");
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    quint64 sequence = 0;
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != SchemaVersion
        || !readPositiveSequence(object, &sequence) || sequence <= m_lastSequence) {
        *error = QStringLiteral("control command has an invalid sequence");
        return std::nullopt;
    }
    const QString action = object.value(QStringLiteral("action")).toString();
    Command command;
    command.sequence = sequence;
    if (action == QStringLiteral("fullscreen")) {
        const QJsonValue enabled = object.value(QStringLiteral("enabled"));
        if (!enabled.isBool()) {
            *error = QStringLiteral("fullscreen command lacks boolean enabled");
            return std::nullopt;
        }
        command.action = Action::Fullscreen;
        command.fullscreen = enabled.toBool();
    } else if (action == QStringLiteral("close")) {
        command.action = Action::Close;
    } else {
        *error = QStringLiteral("control command action is unsupported");
        return std::nullopt;
    }
    m_lastCommand = bytes;
    m_lastSequence = sequence;
    return command;
}

bool Channel::acknowledge(const Command &command, bool fullscreen, QString *error) const
{
    QJsonObject object{{QStringLiteral("schemaVersion"), SchemaVersion},
                       {QStringLiteral("sequence"), static_cast<qint64>(command.sequence)},
                       {QStringLiteral("status"), QStringLiteral("applied")}};
    if (command.action == Action::Fullscreen) {
        object.insert(QStringLiteral("action"), QStringLiteral("fullscreen"));
        object.insert(QStringLiteral("fullscreen"), fullscreen);
    } else {
        object.insert(QStringLiteral("action"), QStringLiteral("close"));
        object.insert(QStringLiteral("closed"), true);
    }
    return writeAcknowledgement(QJsonDocument(object).toJson(QJsonDocument::Compact), error);
}

bool Channel::acknowledgeTimeout(QString *error) const
{
    return writeAcknowledgement(
        QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), SchemaVersion},
                                  {QStringLiteral("status"), QStringLiteral("timed-out")}})
            .toJson(QJsonDocument::Compact),
        error);
}

bool Channel::writeAcknowledgement(const QByteArray &payload, QString *error) const
{
    QSaveFile acknowledgement(m_ackPath);
    if (!acknowledgement.open(QIODevice::WriteOnly)
        || acknowledgement.write(payload) != payload.size() || !acknowledgement.commit()) {
        *error = QStringLiteral("cannot write control acknowledgement");
        return false;
    }
    return true;
}

} // namespace QindaQt::Test::PanelVisibilityControl
