// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QString>

namespace QindaQt::Screenshot {

// Hand-offs to the rest of the desktop. Each returns false with a sentence
// in `error` rather than failing silently.
//
// AGENT-CONTRACT: "Show in folder" uses org.freedesktop.FileManager1
// ShowItems, which the QindaQt File Manager serves (ADR-0289); the Settings
// routes are opened with `qindaqt-settings --page <route>`, the same
// contract the OBS applet uses.
class DesktopActions final {
public:
    explicit DesktopActions(QDBusConnection bus = QDBusConnection::sessionBus());

    bool openFile(const QString &path, QString *error) const;
    bool showInFolder(const QString &path, QString *error) const;
    bool openSettingsPage(const QString &route, QString *error) const;

private:
    QDBusConnection m_bus;
};

} // namespace QindaQt::Screenshot
