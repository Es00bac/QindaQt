// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/game_priority.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <unistd.h>

namespace QindaQt::Controllers {
namespace {
QString readSmall(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.read(4096)).trimmed() : QString{};
}
bool matchesId(const QString &dir, quint16 vendor, quint16 product) {
    bool vOk = false, pOk = false;
    const auto v = readSmall(dir + "/id/vendor").toUInt(&vOk, 16);
    const auto p = readSmall(dir + "/id/product").toUInt(&pOk, 16);
    return vOk && pOk && v == vendor && p == product;
}
}
GamePriorityState inspectGamePriority(const QSet<QString> &paths, const QString &root, qint64 self) {
    if (self < 0) self = QCoreApplication::applicationPid();
    GamePriorityState state;
    const QDir proc(root);
    const auto processes = proc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &process : processes) {
        bool numeric = false;
        const auto pid = process.fileName().toLongLong(&numeric);
        if (!numeric || pid == self || process.ownerId() != ::getuid()) continue;
        const auto name = readSmall(process.absoluteFilePath() + "/comm").toLower();
        if (name == "steam" || name == "steamwebhelper" || name == "steam.exe") state.steam = true;
        if (paths.isEmpty()) continue;
        const QDir fds(process.absoluteFilePath() + "/fd");
        for (const auto &fd : fds.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot)) {
            const auto target = fd.symLinkTarget();
            if (paths.contains(target)) state.busyPaths.insert(target);
        }
    }
    return state;
}
QStringList controllerDevicePaths(quint16 vendor, quint16 product, const QString &sdlPath,
                                 const QString &sysRoot, const QString &devRoot) {
    QSet<QString> paths;
    if (sdlPath.startsWith(devRoot + "/")) paths.insert(sdlPath);
    if (vendor == 0 && product == 0) return paths.values();
    const QDir inputs(sysRoot + "/class/input");
    for (const auto &entry : inputs.entryInfoList({"event*", "js*"}, QDir::Dirs | QDir::System))
        if (matchesId(entry.absoluteFilePath() + "/device", vendor, product))
            paths.insert(devRoot + "/input/" + entry.fileName());
    const QDir hid(sysRoot + "/class/hidraw");
    for (const auto &entry : hid.entryInfoList({"hidraw*"}, QDir::Dirs | QDir::System)) {
        const auto lines = readSmall(entry.absoluteFilePath() + "/device/uevent").split('\n');
        for (const auto &line : lines) {
            if (!line.startsWith("HID_ID=")) continue;
            const auto fields = line.mid(7).split(':');
            if (fields.size() == 3 && fields[1].toUInt(nullptr, 16) == vendor
                && fields[2].toUInt(nullptr, 16) == product)
                paths.insert(devRoot + "/" + entry.fileName());
        }
    }
    return paths.values();
}
} // namespace QindaQt::Controllers
