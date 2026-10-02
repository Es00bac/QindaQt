// SPDX-License-Identifier: LGPL-3.0-or-later
// Account semantics adapted from Jan Grulich / Red Hat xdg-desktop-portal-kde
// account.cpp (2020), LGPL-2.0-or-later. No AccountsService/KDE dependency.
#include "misc_policy.h"
#include <QFileInfo>
#include <QUrl>
#include <pwd.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
AccountInformation localAccountInformation() {
    AccountInformation result; const auto *user = getpwuid(getuid()); if (!user) return result;
    result.id = QString::fromLocal8Bit(user->pw_name);
    result.name = QString::fromLocal8Bit(user->pw_gecos).section(QLatin1Char(','), 0, 0);
    if (result.name.isEmpty()) result.name = result.id;
    const QString home = QString::fromLocal8Bit(user->pw_dir);
    for (const auto *file : {"/.face", "/.face.icon"}) {
        const QFileInfo face(home + QString::fromLatin1(file));
        if (face.isFile() && face.isReadable()) { result.image = QUrl::fromLocalFile(face.absoluteFilePath()).toString(); break; }
    }
    if (result.image.isEmpty()) result.image = QStringLiteral("file://");
    return result;
}
}
