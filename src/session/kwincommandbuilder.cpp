// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwincommandbuilder.h"

#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Session {
namespace {

QStringList fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return {};
}

bool validSocketName(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/'))) {
        return false;
    }
    return std::all_of(name.cbegin(), name.cend(), [](const QChar character) {
        return character.isLetterOrNumber() || character == QLatin1Char('-')
            || character == QLatin1Char('_') || character == QLatin1Char('.');
    });
}

} // namespace

QStringList KWinCommandBuilder::build(const SessionOptions &options, QString *error)
{
    if (options.kwinExecutable.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("KWin executable must not be empty"));
    }
    if (!validSocketName(options.socketName)) {
        return fail(error, QStringLiteral("Wayland socket name is invalid"));
    }
    if (options.outputSize.width() < 320 || options.outputSize.height() < 200) {
        return fail(error, QStringLiteral("output size must be at least 320x200"));
    }
    if (options.outputSize.width() > 32768 || options.outputSize.height() > 32768) {
        return fail(error, QStringLiteral("output size exceeds the compositor safety limit"));
    }
    if (!std::isfinite(options.scale) || options.scale < 0.5 || options.scale > 4.0) {
        return fail(error, QStringLiteral("output scale must be between 0.5 and 4.0"));
    }
    if (options.outputCount < 1 || options.outputCount > 16) {
        return fail(error, QStringLiteral("output count must be between 1 and 16"));
    }
    if (options.backend == Backend::NestedWayland
        && options.parentWaylandDisplay.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("nested Wayland mode requires WAYLAND_DISPLAY"));
    }
    if (!options.testScenario.isEmpty() && !QFileInfo::exists(options.testScenario)) {
        return fail(error,
                    QStringLiteral("test scenario does not exist: %1").arg(options.testScenario));
    }

    QStringList command{options.kwinExecutable};
    switch (options.backend) {
    case Backend::Drm:
        command.append(QStringLiteral("--drm"));
        break;
    case Backend::NestedWayland:
        command.append({QStringLiteral("--wayland-display"), options.parentWaylandDisplay});
        break;
    case Backend::Virtual:
        command.append(QStringLiteral("--virtual"));
        break;
    }

    command.append({QStringLiteral("--socket"),
                    options.socketName,
                    QStringLiteral("--width"),
                    QString::number(options.outputSize.width()),
                    QStringLiteral("--height"),
                    QString::number(options.outputSize.height()),
                    QStringLiteral("--scale"),
                    QString::number(options.scale, 'g', 12),
                    QStringLiteral("--output-count"),
                    QString::number(options.outputCount)});
    if (options.xwayland) {
        command.append(QStringLiteral("--xwayland"));
    }
    if (!options.lockscreen) {
        command.append(QStringLiteral("--no-lockscreen"));
    }
    if (!options.globalShortcuts) {
        command.append(QStringLiteral("--no-global-shortcuts"));
    }
    if (options.replace) {
        command.append(QStringLiteral("--replace"));
    }
    if (!options.sessionExecutable.isEmpty()) {
        command.append({QStringLiteral("--exit-with-session"), options.sessionExecutable});
    }
    return command;
}

} // namespace QindaQt::Session
