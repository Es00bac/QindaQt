// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/shell/icons/icon_runtime.h>

#include <QDir>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QQmlEngine>
#include <QStringList>

namespace QindaQt::Tests
{

// Applet Icon assertions must exercise the resolved provider path. Exact
// icon-name checks alone do not prove that the QML element publishes a source,
// while accepting its placeholder makes the assertion tautological.
inline bool installResolvedIconFixture(QQmlEngine &engine, const QString &root,
                                       const QStringList &names, QString *error)
{
    QDir fixture(root);
    if (fixture.exists() && !fixture.removeRecursively()) {
        *error = QStringLiteral("could not clear icon fixture root");
        return false;
    }

    const QString payload = root + QStringLiteral("/fixturetheme/32");
    if (!QDir().mkpath(payload)) {
        *error = QStringLiteral("could not create icon fixture payload");
        return false;
    }

    QFile index(root + QStringLiteral("/fixturetheme/index.theme"));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || index.write("[Icon Theme]\nName=Applet test\nDirectories=32\n\n"
                       "[32]\nSize=32\nType=Fixed\n") < 0) {
        *error = QStringLiteral("could not write icon fixture index");
        return false;
    }
    index.close();

    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(QColor(240, 244, 241));
    for (const QString &name : names) {
        if (name.isEmpty() || !image.save(payload + QLatin1Char('/') + name
                                              + QStringLiteral(".png"),
                                          "png")) {
            *error = QStringLiteral("could not write icon fixture %1").arg(name);
            return false;
        }
    }

    if (!Shell::Icons::IconRuntime::install(
            engine, {root}, {QStringLiteral("fixturetheme")})) {
        *error = QStringLiteral("could not install icon runtime");
        return false;
    }
    return true;
}

inline bool hasResolvedProviderSource(QObject *icon, const QString &expectedName)
{
    if (icon == nullptr || icon->property("name").toString() != expectedName
        || !icon->property("resolved").toBool()) {
        return false;
    }
    QObject *image = icon->findChild<QObject *>(QStringLiteral("iconImage"));
    return image != nullptr
        && image->property("source").toUrl().toString().startsWith(
            QStringLiteral("image://qindaqt-icon/") + expectedName);
}

} // namespace QindaQt::Tests
