// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

namespace QindaQt::Test::PanelVisibilityControl {

enum class Action {
    Fullscreen,
    Close,
};

struct Command {
    quint64 sequence = 0;
    Action action = Action::Close;
    bool fullscreen = false;
};

// AGENT-CONTRACT: This channel is a test-only file boundary for a probe in an
// isolated nested session. The runner atomically replaces commandPath with one
// schema-v1 command and reads commandPath + ".ack" for the corresponding
// acknowledgement. It is not a compositor control API.
class Channel final {
public:
    explicit Channel(QString commandPath);

    [[nodiscard]] bool isValid(QString *error) const;
    [[nodiscard]] bool publishReady(const QString &title, QString *error) const;
    [[nodiscard]] std::optional<Command> readNext(QString *error);
    [[nodiscard]] bool acknowledge(const Command &command, bool fullscreen,
                                   QString *error) const;
    [[nodiscard]] bool acknowledgeTimeout(QString *error) const;

private:
    [[nodiscard]] bool writeAcknowledgement(const QByteArray &payload,
                                            QString *error) const;

    QString m_commandPath;
    QString m_ackPath;
    QByteArray m_lastCommand;
    quint64 m_lastSequence = 0;
};

} // namespace QindaQt::Test::PanelVisibilityControl
