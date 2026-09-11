// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/new_folder_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace QindaQt::Shell::DesktopSurface {
namespace {

constexpr int MaxGeneratedCandidate = 1000;
constexpr int MaxNameLength = 255;

// See the AGENT-GUARD in the header: plain file names only, regardless of
// where the name came from.
bool isAcceptableName(const QString &name)
{
    if (name.isEmpty() || name.length() > MaxNameLength) {
        return false;
    }
    if (name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }
    if (name.startsWith(QLatin1Char('.'))) {
        return false;
    }
    return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))
        && !QFileInfo(name).isAbsolute();
}

// One past the highest existing "New Folder"/"New Folder N" name, or empty
// when the scan is exhausted (pathological Desktop states must not loop
// forever). Generated names are never reused: deleting "New Folder 2" out of
// a 1–3 sequence still yields "New Folder 4" next, which keeps a
// just-created folder's identity stable for the file-manager handoff.
QString uniqueGeneratedName(const QDir &desktop)
{
    const QString base = QStringLiteral("New Folder");
    int highest = desktop.exists(base) ? 1 : 0;
    for (int candidate = 2; candidate <= MaxGeneratedCandidate; ++candidate) {
        if (desktop.exists(QStringLiteral("%1 %2").arg(base).arg(candidate))) {
            highest = candidate;
        }
    }
    if (highest >= MaxGeneratedCandidate) {
        return {};
    }
    return highest == 0 ? base
                        : QStringLiteral("%1 %2").arg(base).arg(highest + 1);
}

} // namespace

NewFolderController::NewFolderController(QObject *parent) : QObject(parent) {}

QString NewFolderController::create(const QString &requestedName)
{
    const QString desktop =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty()) {
        publishFeedback(
            QStringLiteral("The Desktop directory is unavailable"));
        return {};
    }
    QString name = requestedName;
    if (name.isEmpty()) {
        name = uniqueGeneratedName(QDir(desktop));
        if (name.isEmpty()) {
            publishFeedback(QStringLiteral(
                "The Desktop has no free \"New Folder\" name left"));
            return {};
        }
    }
    if (!isAcceptableName(name)) {
        publishFeedback(QStringLiteral("That folder name is not allowed"));
        return {};
    }
    const QDir desktopDir(desktop);
    if (!desktopDir.mkpath(name)) {
        publishFeedback(QStringLiteral("Could not create %1").arg(name));
        return {};
    }
    clearFeedback();
    return desktopDir.absoluteFilePath(name);
}

void NewFolderController::clearFeedback()
{
    publishFeedback({});
}

void NewFolderController::publishFeedback(const QString &message)
{
    if (m_feedback == message) {
        return;
    }
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopSurface
