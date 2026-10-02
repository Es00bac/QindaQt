// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
namespace QindaQt::Services::Portal {
// Linked into the non-installable actual-input broker only. Read each action
// after the actual frontend request, so a long-lived broker does not inherit
// stale parent environment. Nothing here grants privilege or fabricates data.
bool augmentCaptureTestFrame(QJsonObject &frame) {
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QFile input(QDir(runtime).filePath("qindaqt-capture-input.json"));
    if (!input.open(QIODevice::ReadOnly) || input.size() > 1024) return false;
    QJsonParseError error; const auto object = QJsonDocument::fromJson(input.readAll(), &error).object();
    const auto action = object.value("action").toString();
    if (error.error != QJsonParseError::NoError || object.size() != 1 || (action != "allow" && action != "allow-remember" && action != "allow-restore" && action != "cancel" && action != "hold")) return false;
    frame.insert("test_action", action); frame.insert("test_audit", QDir(runtime).filePath("qindaqt-capture.audit")); return true;
}
}
