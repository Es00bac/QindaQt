// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_presenter.h>

namespace QindaQt::Link {

// Starts `qindaqt-link --reply <device> <key> <action>` when the QindaLink app
// is installed (its org.qindaqt.Link.desktop file exists). Without the app
// available() is false and the feature omits reply actions from the desktop
// notification. The program name is injectable for tests.
class ProcessReplyLauncher final : public ReplyLauncher {
public:
    explicit ProcessReplyLauncher(QString program = QStringLiteral("qindaqt-link"),
                                  QString desktopFile = QStringLiteral("org.qindaqt.Link.desktop"))
        : m_program(std::move(program)), m_desktopFile(std::move(desktopFile))
    {
    }
    [[nodiscard]] bool available() const override;
    void launch(const QString &deviceId, const QString &key, const QString &actionId) override;

private:
    QString m_program, m_desktopFile;
};

} // namespace QindaQt::Link
