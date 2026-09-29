#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Compositor::KWinIntegration {

struct MinimizedApplicationIdentity final
{
    QString applicationId;
    QString label;
    QStringList iconThemeCandidates;
};

// Resolves display identity from an installed desktop entry first, then
// compositor metadata, while retaining the window caption as a useful fallback.
[[nodiscard]] MinimizedApplicationIdentity resolveMinimizedApplicationIdentity(
    const QString &desktopFileName,
    const QString &resourceClass,
    const QString &desktopName,
    const QString &desktopIconName,
    const QString &caption);

} // namespace QindaQt::Compositor::KWinIntegration
