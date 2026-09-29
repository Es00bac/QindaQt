// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_actions.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

namespace QindaQt::Screenshot {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Screenshot", text);
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

} // namespace

DesktopActions::DesktopActions(QDBusConnection bus)
    : m_bus(std::move(bus))
{
}

bool DesktopActions::openFile(const QString &path, QString *error) const
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setError(error, tr("The file is no longer there."));
        return false;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        setError(error, tr("No application opened %1.").arg(QFileInfo(path).fileName()));
        return false;
    }
    return true;
}

bool DesktopActions::showInFolder(const QString &path, QString *error) const
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setError(error, tr("The file is no longer there."));
        return false;
    }
    if (m_bus.isConnected()) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
            QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
        message.setArguments({QStringList{QUrl::fromLocalFile(path).toString()}, QString()});
        // Fire and forget: the file manager may take a moment to start, and
        // the fallback below would open a second window if we waited badly.
        if (m_bus.send(message))
            return true;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()))) {
        setError(error, tr("The folder could not be opened."));
        return false;
    }
    return true;
}

bool DesktopActions::openSettingsPage(const QString &route, QString *error) const
{
    if (!QProcess::startDetached(QStringLiteral("qindaqt-settings"), {QStringLiteral("--page"), route})) {
        setError(error, tr("Settings could not be opened."));
        return false;
    }
    return true;
}

} // namespace QindaQt::Screenshot
