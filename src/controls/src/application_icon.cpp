// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/controls/application_icon.h>
#include <QFile>
#include <QRegularExpression>

namespace QindaQt::Controls {
QIcon applicationIcon(const QString &name) {
    static const QRegularExpression valid(QStringLiteral("^[a-zA-Z0-9][a-zA-Z0-9._-]{0,127}$"));
    if (!valid.match(name).hasMatch())
        return {};
    QIcon fallback;
    for (const auto *group : {"actions", "apps", "places", "mimetypes", "devices", "status", "categories"}) {
        const QString path = QStringLiteral(":/qindaqt/icons/%1/%2.svg")
                                 .arg(QLatin1String(group), name);
        if (QFile::exists(path)) {
            fallback = QIcon(path);
            break;
        }
    }
    return QIcon::fromTheme(name, fallback);
}
}
