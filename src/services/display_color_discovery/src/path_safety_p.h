// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>

namespace QindaQt::DisplayColor
{

// AGENT-GUARD: QFileInfo::isSymLink checks only its final component. Injected
// roots must reject every parent redirect before enumeration or import resolves
// the path outside the caller-authorized directory chain.
inline bool injectedRootHasSymlinkedAncestor(const QString &path)
{
    QString current = QFileInfo(path).absolutePath();
    while (!current.isEmpty()) {
        if (QFileInfo(current).isSymLink()) {
            return true;
        }
        const QString parent = QFileInfo(current).dir().absolutePath();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return false;
}

} // namespace QindaQt::DisplayColor
