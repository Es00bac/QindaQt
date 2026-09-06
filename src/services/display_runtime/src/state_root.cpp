// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_runtime/state_root.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace QindaQt::DisplayRuntime
{
namespace
{

constexpr qsizetype kMaximumStateRootBytes = 4'096;

bool safeText(const QString &value)
{
    if (value.isEmpty() || value.toUtf8().size() > kMaximumStateRootBytes) {
        return false;
    }
    for (const QChar character : value) {
        if (character.isNull() || character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

StateRootSelection validate(QString path)
{
    if (!safeText(path) || !QDir::isAbsolutePath(path)
        || path == QStringLiteral("/") || QDir::cleanPath(path) != path) {
        return {.path = {},
                .error = StateRootError::InvalidPath,
                .reasonCode = QStringLiteral("invalid-state-root-path")};
    }
    return {.path = std::move(path),
            .error = StateRootError::None,
            .reasonCode = {}};
}

} // namespace

StateRootSelection selectStateRoot(const StateRootInputs &inputs)
{
    if (!inputs.explicitPath.isEmpty()) {
        return validate(inputs.explicitPath);
    }
    if (!inputs.systemdStateDirectory.isEmpty()) {
        if (inputs.systemdStateDirectory.contains(QLatin1Char(':'))) {
            return {.path = {},
                    .error = StateRootError::AmbiguousSystemdDirectory,
                    .reasonCode = QStringLiteral("ambiguous-state-directory")};
        }
        return validate(inputs.systemdStateDirectory);
    }
    if (!inputs.xdgStateHome.isEmpty()) {
        const StateRootSelection base = validate(inputs.xdgStateHome);
        if (!base.accepted()) {
            return base;
        }
        return validate(QDir(base.path).filePath(QStringLiteral("qindaqt")));
    }
    if (!inputs.home.isEmpty()) {
        const StateRootSelection base = validate(inputs.home);
        if (!base.accepted()) {
            return base;
        }
        return validate(QDir(base.path).filePath(
            QStringLiteral(".local/state/qindaqt")));
    }
    return {.path = {},
            .error = StateRootError::Missing,
            .reasonCode = QStringLiteral("missing-state-root")};
}

StateRootSelection
resolveProvisionedStateRoot(const StateRootSelection &selection)
{
    if (!selection.accepted()) {
        return selection;
    }
    const QString canonical = QFileInfo(selection.path).canonicalFilePath();
    if (canonical.isEmpty()) {
        return {.path = {},
                .error = StateRootError::InvalidPath,
                .reasonCode = QStringLiteral("invalid-state-root-path")};
    }
    return validate(canonical);
}

} // namespace QindaQt::DisplayRuntime
