// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <algorithm>

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
inline QString textPdf(const QString &directory)
{
    const QString path = directory + QStringLiteral("/unicode text.pdf");
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QSizeF(120, 160), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    QFont font(QStringLiteral("DejaVu Sans")); font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::black);
    const QString first = QStringLiteral("Alpha zero\n<b>literal & café</b>\nalpha end.\n")
        + QStringLiteral("Selectable page text with keyboard scrolling.\n").repeated(20);
    painter.drawText(QRect(12, 12, writer.width() - 24, writer.height() - 24),
                     Qt::AlignLeft | Qt::TextWordWrap, first);
    writer.newPage();
    painter.drawText(QRect(12, 12, writer.width() - 24, writer.height() - 24),
                     Qt::AlignLeft | Qt::TextWordWrap, QStringLiteral("BETA second page\nAlpha final"));
    painter.end();
    return path;
}
// Minimal self-authored PDF objects let limit tests exercise the real parser
// without a giant committed asset or a fixture-generation package dependency.
inline QString boundedTextPdf(const QString &directory, int pages, int extractedUnits)
{
    QList<QByteArray> objects;
    QByteArray children;
    for (int page = 0; page < pages; ++page)
        children += QByteArray::number(5 + page) + " 0 R ";
    objects.append("<< /Type /Catalog /Pages 2 0 R >>");
    objects.append("<< /Type /Pages /Count " + QByteArray::number(pages)
                   + " /Kids [" + children + "] >>");
    objects.append("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    // AGENT-NOTE: Poppler drops tiny glyphs after 50000 characters per page.
    // Ordinary-size, fully in-bounds lines make this a real extraction-limit
    // fixture. ReadingOrder adds one newline per line and one per flow.
    Q_ASSERT(extractedUnits == 0 || extractedUnits == 262144 || extractedUnits == 262145);
    const int rows = extractedUnits == 0 ? 0 : (extractedUnits + 1022) / 1024;
    int letters = extractedUnits == 0 ? 0 : extractedUnits - rows - 1;
    QByteArray content("BT /F1 10 Tf 14 TL 10 3990 Td\n");
    for (int row = 0; row < rows; ++row) {
        const int count = std::min(1023, letters);
        content += "(" + QByteArray(count, 'a') + ") Tj\n";
        letters -= count;
        if (row + 1 < rows) content += "T*\n";
    }
    content += "ET\n";
    objects.append("<< /Length " + QByteArray::number(content.size())
                   + " >>\nstream\n" + content + "endstream");
    for (int page = 0; page < pages; ++page)
        objects.append("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 6000 4000] "
                       "/Resources << /Font << /F1 3 0 R >> >> /Contents 4 0 R >>");
    QByteArray data("%PDF-1.7\n% SPDX-License-Identifier: GPL-3.0-or-later\n");
    QList<qsizetype> offsets;
    for (qsizetype index = 0; index < objects.size(); ++index) {
        offsets.append(data.size());
        data += QByteArray::number(index + 1) + " 0 obj\n" + objects[index] + "\nendobj\n";
    }
    const qsizetype xref = data.size();
    data += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const auto offset : offsets)
        data += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    data += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1)
        + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    const QString path = directory + QStringLiteral("/bounded-text.pdf");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return {};
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
