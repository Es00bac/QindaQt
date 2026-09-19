// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>

namespace ViewerFixtures {
inline QString pdf(const QString &directory)
{
    const QString path = directory + QStringLiteral("/two pages.pdf");
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QSizeF(120, 80), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    painter.fillRect(QRect(0, 0, writer.width(), writer.height()), Qt::red);
    writer.newPage();
    painter.fillRect(QRect(0, 0, writer.width(), writer.height()), Qt::blue);
    painter.end();
    return path;
}
inline QString image(const QString &directory, const char *format = "png")
{
    const QString path = directory + QStringLiteral("/sample image.") + QString::fromLatin1(format);
    QImage value(120, 80, QImage::Format_ARGB32);
    value.fill(Qt::green);
    QPainter painter(&value);
    painter.fillRect(0, 0, 60, 80, Qt::red);
    painter.end();
    return value.save(path, format) ? path : QString();
}
inline QString bytes(const QString &directory, const QString &name, const QByteArray &data)
{
    const QString path = directory + QLatin1Char('/') + name;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return {};
    return path;
}
} // namespace ViewerFixtures
