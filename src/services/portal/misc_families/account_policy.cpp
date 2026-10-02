// SPDX-License-Identifier: LGPL-3.0-or-later
// Account semantics adapted from Jan Grulich / Red Hat xdg-desktop-portal-kde
// account.cpp (2020), LGPL-2.0-or-later. No AccountsService/KDE dependency.
#include "misc_policy.h"
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <pwd.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
AccountInformation localAccountInformation() {
    AccountInformation result; const auto *user = getpwuid(getuid()); if (!user) return result;
    result.id = QString::fromLocal8Bit(user->pw_name);
    result.name = QString::fromLocal8Bit(user->pw_gecos).section(QLatin1Char(','), 0, 0);
    if (result.name.isEmpty()) result.name = result.id;
    const QString home = QString::fromLocal8Bit(user->pw_dir);
    for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        for (const auto *theme : {"QindaQt-Handdrawn", "QindaQt-Breeze-Painted"}) {
            const QFileInfo icon(QDir(root).filePath(QStringLiteral("icons/") + QString::fromLatin1(theme) + QStringLiteral("/128x128/actions/user-identity.png")));
            if (icon.isFile() && icon.isReadable()) { result.defaultImage = QUrl::fromLocalFile(icon.canonicalFilePath()).toString(); break; }
        }
        if (!result.defaultImage.isEmpty()) break;
    }
    for (const auto *file : {"/.face", "/.face.icon"}) {
        const QFileInfo face(home + QString::fromLatin1(file));
        if (face.isFile() && face.isReadable()) { result.image = QUrl::fromLocalFile(face.canonicalFilePath()).toString(); break; }
    }
    // AGENT-CONTRACT: The frontend exports this file through Documents and
    // requires an existing avatar. A generic installed Qinda icon replaces an
    // absent or withheld personal photo; file:// alone is not a valid image.
    if (result.image.isEmpty()) result.image = result.defaultImage;
    if (result.defaultImage.isEmpty() || result.image.isEmpty()) return {};
    return result;
}
}
