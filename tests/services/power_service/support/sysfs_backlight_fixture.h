// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtTest>

namespace QindaQt::Tests {

// A temporary `/sys/class/backlight`-shaped tree. Every backlight row builds
// its devices through this so no test ever reads or writes the host's real
// class directory.
struct BacklightFixture
{
    explicit BacklightFixture(const QTemporaryDir &root)
        : rootPath(root.path())
    {
    }

    QString makeDevice(const QString &name, const QByteArray &type,
                       const QByteArray &maximum, const QByteArray &brightness,
                       const QByteArray &actual = QByteArray())
    {
        const QString directory = rootPath + QLatin1Char('/') + name;
        if (!QDir().mkpath(directory)) {
            return {};
        }
        write(directory + QStringLiteral("/type"), type);
        write(directory + QStringLiteral("/max_brightness"), maximum);
        write(directory + QStringLiteral("/brightness"), brightness);
        if (!actual.isNull()) {
            write(directory + QStringLiteral("/actual_brightness"), actual);
        }
        return directory;
    }

    static void write(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
        file.close();
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    QString rootPath;
};

} // namespace QindaQt::Tests
