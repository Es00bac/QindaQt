// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QRawFont>
#include <QStringList>
#include <QTest>

using QindaQt::Controls::TestSupport::pinDeterministicFonts;

namespace {

// AGENT-CONTRACT: The Controls visual baselines are only truthful while the
// schema's "Inter" family resolves to the vendored repository-owned font
// bytes. The vendored files declare the repository-owned family
// "QindaQt Sans" (no host font can declare it), so this row fails when the
// fixture registration is removed, renamed, or reordered, and it fails if a
// Qt update ever stopped application fonts from answering the substituted
// family — the same drift would otherwise silently re-render every reviewed
// baseline from host bytes.
//
// AGENT-NOTE: QFont matching case-folds family names, so the resolved engine
// family is compared case-insensitively while the registered fixture is
// verified with exact comparisons inside pinDeterministicFonts().
[[nodiscard]] QByteArray nameTableFor(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    const auto be32 = [](const char *p) {
        return (static_cast<quint32>(static_cast<quint8>(p[0])) << 24)
            | (static_cast<quint32>(static_cast<quint8>(p[1])) << 16)
            | (static_cast<quint32>(static_cast<quint8>(p[2])) << 8)
            | static_cast<quint32>(static_cast<quint8>(p[3]));
    };
    // AGENT-GUARD: numTables is the big-endian uint16 at offset 4 of the sfnt
    // header; reading a wider field here would let a malformed fixture drive
    // this scan past the end of the file buffer instead of failing cleanly.
    const quint32 numTables = (static_cast<quint32>(static_cast<quint8>(bytes.at(4))) << 8)
        | static_cast<quint8>(bytes.at(5));
    for (quint32 index = 0; index < numTables; ++index) {
        const char *record = bytes.constData() + 12 + 16 * index;
        if (qstrncmp(record, "name", 4) == 0) {
            return bytes.mid(static_cast<qsizetype>(be32(record + 8)),
                             static_cast<qsizetype>(be32(record + 12)));
        }
    }
    return {};
}

} // namespace

class ControlsFontPinningTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void substitutionResolvesVendoredBytes();
};

void ControlsFontPinningTests::initTestCase()
{
    pinDeterministicFonts();
}

void ControlsFontPinningTests::substitutionResolvesVendoredBytes()
{
    QFont request(QStringLiteral("Inter"));
    request.setPixelSize(14);
    const QRawFont resolved = QRawFont::fromFont(request);
    QVERIFY2(resolved.isValid(), "engine did not resolve the substituted family");
    QVERIFY2(resolved.familyName().compare(QStringLiteral("QindaQt Sans"),
                                           Qt::CaseInsensitive)
                 == 0,
             qPrintable(QStringLiteral("substitution resolved to family %1")
                            .arg(resolved.familyName())));
    QCOMPARE(resolved.styleName(), QStringLiteral("Regular"));

    const QString regularPath =
        QStringLiteral(QINDAQT_CONTROLS_FONT_DIR "/NotoSans-Regular.ttf");
    const QByteArray expectedTable = nameTableFor(regularPath);
    QVERIFY2(!expectedTable.isEmpty(),
             "vendored regular fixture has no name table");
    QCOMPARE(resolved.fontTable("name"), expectedTable);
}

QTEST_MAIN(ControlsFontPinningTests)
#include "tst_controls_font_pinning.moc"
