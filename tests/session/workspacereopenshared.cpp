// SPDX-License-Identifier: GPL-3.0-or-later
#include "workspacereopenphases.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QThread>

#include <utility>

namespace QindaQt::Test {
namespace {

// The client helper reuses this same executable; the environment selects its
// application identity and windows so every fixture window is a real Wayland
// client of the nested compositor.
constexpr auto ClientAppIdVariable = "QINDAQT_WREOPEN_APP_ID";
constexpr auto ClientTitlesVariable = "QINDAQT_WREOPEN_TITLES";
constexpr auto ClientSizesVariable = "QINDAQT_WREOPEN_SIZES";

} // namespace

QVector<QProcess *> spawnWorkspaceClients(
    const QVector<WorkspaceClientSpec> &specs, const QStringList &activateTitles,
    QObject *parent, QString *error)
{
    QVector<QProcess *> processes;
    processes.reserve(qsizetype(specs.size()));
    for (const auto &spec : specs) {
        if (spec.desktopEntryId.isEmpty()
            || spec.titles.size() != spec.sizes.size() || spec.titles.isEmpty()) {
            *error = QStringLiteral(
                "workspace client spec for '%1' is inconsistent")
                         .arg(spec.desktopEntryId);
            qDeleteAll(processes);
            return {};
        }
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QString::fromLatin1(ClientAppIdVariable),
                           spec.desktopEntryId);
        environment.insert(QString::fromLatin1(ClientTitlesVariable),
                           spec.titles.join(QLatin1Char(';')));
        QStringList sizes;
        for (const QSize &size : spec.sizes) {
            sizes.append(QStringLiteral("%1x%2").arg(size.width())
                             .arg(size.height()));
        }
        environment.insert(QString::fromLatin1(ClientSizesVariable),
                           sizes.join(QLatin1Char(';')));
        // Titles listed here raise themselves once mapped so later pointer
        // presses land on a known topmost client despite virtual placement
        // stacking every new window at the same origin.
        environment.insert(QStringLiteral("QINDAQT_WREOPEN_ACTIVATE"),
                           activateTitles.join(QLatin1Char(';')));
        auto *const process = new QProcess(parent);
        process->setProcessEnvironment(environment);
        process->setProgram(QCoreApplication::applicationFilePath());
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->start();
        if (!process->waitForStarted(3000)) {
            *error = QStringLiteral("workspace client %1 did not start: %2")
                         .arg(spec.desktopEntryId, process->errorString());
            qDeleteAll(processes);
            return {};
        }
        processes.append(process);
    }
    return processes;
}

std::optional<QJsonObject> readSavedWorkspaceDocument(QString *error)
{
    const auto root =
        QString::fromUtf8(qgetenv("XDG_DATA_HOME")) + QStringLiteral("/qindaqt/workspaces");
    // The atomic store write is synchronous on the compositor GUI thread, but
    // allow a short poll so the probe never races a late flush.
    for (int attempt = 0; attempt < 50; ++attempt) {
        const QDir directory(root);
        const auto files = directory.entryList({QStringLiteral("*.json")},
                                               QDir::Files, QDir::Name);
        if (files.size() == 1) {
            QFile file(directory.absoluteFilePath(files.constFirst()));
            if (!file.open(QIODevice::ReadOnly)) {
                *error = QStringLiteral("could not read %1").arg(file.fileName());
                return std::nullopt;
            }
            const auto document = QJsonDocument::fromJson(file.readAll());
            if (!document.isObject()) {
                *error = QStringLiteral("saved workspace %1 is not a JSON object")
                             .arg(files.constFirst());
                return std::nullopt;
            }
            return document.object();
        }
        if (files.size() > 1) {
            *error = QStringLiteral(
                "workspace store holds %1 documents; exactly one was saved")
                         .arg(files.size());
            return std::nullopt;
        }
        QThread::msleep(100);
    }
    *error = QStringLiteral("no workspace document appeared below %1").arg(root);
    return std::nullopt;
}

} // namespace QindaQt::Test
