// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QPair>
#include <QRawFont>
#include <QStringList>
#include <QTest>

using QindaQt::Controls::TestSupport::pinDeterministicFonts;

namespace {

// AGENT-CONTRACT: The Controls visual baselines are only truthful while every
// family a theme can request resolves to the vendored repository-owned font
// bytes. The vendored files declare the repository-owned families
// "QindaQt Sans"/"QindaQt Sans Mono" (no host font can declare them), and
// pinDeterministicFonts() rewrites every theme catalog family onto them, so
// these rows fail when the fixture registration, the theme rewriting, or the
// name-table rewrite is removed, renamed, or reordered, and they fail if a Qt
// update ever stopped application fonts from answering the registered
// families — the same drift would otherwise silently re-render every reviewed
// baseline from host bytes.
//
// AGENT-NOTE: QFont matching case-folds family names, so the resolved engine
// family is compared case-insensitively while the registered fixture is
// verified with exact comparisons inside pinDeterministicFonts().
constexpr struct {
    const char *fileName;
    const char *family;
} kVendoredFiles[] = {
    {"NotoSans-Regular.ttf", "QindaQt Sans"},
    {"NotoSans-SemiBold.ttf", "QindaQt Sans"},
    {"NotoSans-Bold.ttf", "QindaQt Sans"},
    {"NotoSansMono-Regular.ttf", "QindaQt Sans Mono"},
};

constexpr quint32 kWholeFontChecksum = 0xB1B0AFBAU;

[[nodiscard]] quint32 be32(const char *p)
{
    return (quint32(quint8(p[0])) << 24) | (quint32(quint8(p[1])) << 16)
        | (quint32(quint8(p[2])) << 8) | quint32(quint8(p[3]));
}

[[nodiscard]] QString vendoredPath(const char *fileName)
{
    return QStringLiteral(QINDAQT_CONTROLS_FONT_DIR "/") + QString::fromLatin1(fileName);
}

// The family names the shipped theme catalog requests, paired with the
// repository-owned family each one must be redirected to. Read from the
// *source* catalog, not the pinned copies, because the pinned copies already
// name the vendored families.
[[nodiscard]] QList<QPair<QString, QString>> catalogSubstitutionTargets()
{
    const QString sourceDir = QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes");
    QList<QPair<QString, QString>> pairs;
    const QStringList catalog = QDir(sourceDir).entryList({QStringLiteral("*.json")},
                                                          QDir::Files, QDir::Name);
    for (const QString &fileName : catalog) {
        QFile file(sourceDir + QLatin1Char('/') + fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QJsonParseError parseError{};
        const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject theme = document.object();
        const QPair<const char *, const char *> keys[] = {
            {"fontFamily", "QindaQt Sans"},
            {"monoFontFamily", "QindaQt Sans Mono"},
        };
        for (const auto &key : keys) {
            const QString family = theme.value(QLatin1String(key.first)).toString();
            if (family.isEmpty()) {
                continue;
            }
            const QPair<QString, QString> pair{family,
                                               QString::fromLatin1(key.second)};
            if (!pairs.contains(pair)) {
                pairs.push_back(pair);
            }
        }
    }
    return pairs;
}

[[nodiscard]] QByteArray nameTableFor(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    // AGENT-GUARD: numTables is the big-endian uint16 at offset 4 of the sfnt
    // header; reading a wider field here would let a malformed fixture drive
    // this scan past the end of the file buffer instead of failing cleanly.
    const quint32 numTables = (quint32(quint8(bytes.at(4))) << 8) | quint8(bytes.at(5));
    for (quint32 index = 0; index < numTables; ++index) {
        const char *record = bytes.constData() + 12 + 16 * index;
        if (qstrncmp(record, "name", 4) == 0) {
            return bytes.mid(qsizetype(be32(record + 8)), qsizetype(be32(record + 12)));
        }
    }
    return {};
}

[[nodiscard]] quint32 sfntChecksum(const char *data, qsizetype length)
{
    quint32 total = 0;
    qsizetype index = 0;
    for (; index + 4 <= length; index += 4) {
        total += be32(data + index);
    }
    if (index < length) {
        quint32 tail = 0;
        int shift = 24;
        while (index < length) {
            tail |= quint32(quint8(data[index])) << shift;
            ++index;
            shift -= 8;
        }
        total += tail;
    }
    return total;
}

} // namespace

class ControlsFontPinningTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void substitutionResolvesVendoredBytes();
    void registeredFamiliesServeVendoredBytes();
    void pinnedThemeCatalogNamesOnlyRegisteredFamilies();
    void vendoredFixturesCarryValidSfntChecksums();
};

void ControlsFontPinningTests::initTestCase()
{
    pinDeterministicFonts();
}

// AGENT-GUARD: this row must never name a concrete real-world font family as
// its probe, and must never ask the host which families exist.
// `QFont::insertSubstitution` is consulted only when the engine cannot resolve
// the requested family (see control_test_support.cpp), and what the engine can
// resolve is not knowable from `QFontDatabase::families()`: fontconfig
// aliasing resolves names that are not listed there. Two concrete probes have
// already gone red for reasons that had nothing to do with QindaQt — "Inter",
// after the host installed Inter under ~/.local/share/fonts, and
// "JetBrains Mono", which fontconfig resolves to itself while it is absent
// from the family list. The probes are therefore synthetic families no font
// can declare, one per substitution target, so this row proves the redirection
// mechanism and nothing about the machine it runs on.
//
// The product guarantee that no theme family can render host bytes is asserted
// separately by pinnedThemeCatalogNamesOnlyRegisteredFamilies(): the pinned
// theme copies name only repository-owned families, so the fixture never
// requests a host family in the first place. Substitution is the belt to that
// set of braces, and this row tests the belt.
void ControlsFontPinningTests::substitutionResolvesVendoredBytes()
{
    struct Probe {
        const char *absentFamily;
        const char *vendoredFamily;
        const char *vendoredFile;
    };
    // Suffixed with a fixed nonce so no future host font can collide.
    constexpr Probe probes[] = {
        {"QindaQt Absent Probe Sans 8f3c1d", "QindaQt Sans",
         "NotoSans-Regular.ttf"},
        {"QindaQt Absent Probe Mono 8f3c1d", "QindaQt Sans Mono",
         "NotoSansMono-Regular.ttf"},
    };

    for (const Probe &probe : probes) {
        const QString absent = QString::fromLatin1(probe.absentFamily);
        const QString vendored = QString::fromLatin1(probe.vendoredFamily);
        const QByteArray expectedTable = nameTableFor(vendoredPath(probe.vendoredFile));
        QVERIFY2(!expectedTable.isEmpty(),
                 qPrintable(QStringLiteral("vendored fixture %1 has no name table")
                                .arg(QString::fromLatin1(probe.vendoredFile))));

        QFont::insertSubstitution(absent, vendored);
        QFont request(absent);
        request.setPixelSize(14);
        const QRawFont resolved = QRawFont::fromFont(request);
        QVERIFY2(resolved.isValid(),
                 qPrintable(QStringLiteral("engine did not resolve %1").arg(absent)));
        QVERIFY2(resolved.familyName().compare(vendored, Qt::CaseInsensitive) == 0,
                 qPrintable(QStringLiteral("%1 resolved to family %2, expected %3")
                                .arg(absent, resolved.familyName(), vendored)));
        QCOMPARE(resolved.styleName(), QStringLiteral("Regular"));
        // The bytes, not just the name: a host face that happened to claim the
        // vendored family would still fail here.
        QCOMPARE(resolved.fontTable("name"), expectedTable);
    }

    // The belt is also actually fastened: every family the shipped catalog
    // requests has a substitution registered onto the matching vendored
    // family. `QFont::substitutes` reads the table directly, so this holds
    // whether or not the host has the family installed — which is exactly the
    // host-independence the probes above are built for.
    const QList<QPair<QString, QString>> targets = catalogSubstitutionTargets();
    QVERIFY2(!targets.isEmpty(), "the shipped theme catalog names no font families");
    for (const auto &target : targets) {
        const QStringList registered = QFont::substitutes(target.first);
        QVERIFY2(registered.contains(target.second, Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("catalog family %1 has substitutes [%2], "
                                           "expected to include %3")
                                .arg(target.first,
                                     registered.join(QLatin1String(", ")),
                                     target.second)));
    }
}

void ControlsFontPinningTests::registeredFamiliesServeVendoredBytes()
{
    struct FamilyCheck {
        const char *family;
        const char *fixture;
    };
    const FamilyCheck checks[] = {
        {"QindaQt Sans", "NotoSans-Regular.ttf"},
        {"QindaQt Sans Mono", "NotoSansMono-Regular.ttf"},
    };
    for (const FamilyCheck &check : checks) {
        QFont request(QString::fromLatin1(check.family));
        request.setPixelSize(14);
        const QRawFont resolved = QRawFont::fromFont(request);
        QVERIFY2(resolved.isValid(),
                 qPrintable(QStringLiteral("engine did not resolve %1").arg(check.family)));
        QVERIFY2(resolved.familyName().compare(QString::fromLatin1(check.family),
                                               Qt::CaseInsensitive)
                     == 0,
                 qPrintable(QStringLiteral("%1 resolved to family %2")
                                .arg(QString::fromLatin1(check.family),
                                     resolved.familyName())));
        const QByteArray expectedTable = nameTableFor(vendoredPath(check.fixture));
        QVERIFY2(!expectedTable.isEmpty(),
                 qPrintable(QStringLiteral("vendored fixture has no name table: %1")
                                .arg(check.fixture)));
        QCOMPARE(resolved.fontTable("name"), expectedTable);
    }
}

void ControlsFontPinningTests::pinnedThemeCatalogNamesOnlyRegisteredFamilies()
{
    const QString sourceDir = QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes");
    const QStringList catalog =
        QDir(sourceDir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVERIFY2(!catalog.isEmpty(), "the product theme catalog is empty or missing");

    for (const QString &fileName : catalog) {
        QFile sourceFile(sourceDir + QLatin1Char('/') + fileName);
        QVERIFY2(sourceFile.open(QIODevice::ReadOnly),
                 qPrintable(sourceFile.fileName()));
        QJsonParseError sourceError = {};
        const QJsonDocument source =
            QJsonDocument::fromJson(sourceFile.readAll(), &sourceError);
        QVERIFY2(sourceError.error == QJsonParseError::NoError && source.isObject(),
                 qPrintable(fileName));

        QFile pinnedFile(QStringLiteral(QINDAQT_CONTROLS_PINNED_THEME_DIR "/") + fileName);
        QVERIFY2(pinnedFile.open(QIODevice::ReadOnly),
                 qPrintable(QStringLiteral("pinned theme copy is missing: %1")
                                .arg(pinnedFile.fileName())));
        QJsonParseError pinnedError = {};
        const QJsonDocument pinned =
            QJsonDocument::fromJson(pinnedFile.readAll(), &pinnedError);
        QVERIFY2(pinnedError.error == QJsonParseError::NoError && pinned.isObject(),
                 qPrintable(fileName));

        QJsonObject pinnedTheme = pinned.object();
        QCOMPARE(pinnedTheme.value(QStringLiteral("fontFamily")).toString(),
                 QStringLiteral("QindaQt Sans"));
        QCOMPARE(pinnedTheme.value(QStringLiteral("monoFontFamily")).toString(),
                 QStringLiteral("QindaQt Sans Mono"));

        // AGENT-GUARD: Rewriting the families must not alter any other theme
        // field; a drift here changes what the visual rows actually render.
        QJsonObject expectedOther = source.object();
        expectedOther.remove(QStringLiteral("fontFamily"));
        expectedOther.remove(QStringLiteral("monoFontFamily"));
        pinnedTheme.remove(QStringLiteral("fontFamily"));
        pinnedTheme.remove(QStringLiteral("monoFontFamily"));
        QCOMPARE(pinnedTheme, expectedOther);
    }
}

void ControlsFontPinningTests::vendoredFixturesCarryValidSfntChecksums()
{
    // AGENT-CONTRACT: The renewal tool (rename_family_names.py) rebuilds the
    // sfnt container, so it must also restore every table directory checksum
    // and head.checkSumAdjustment. Qt/FreeType load permissively, so an
    // invalid checksum does not fail on its own; this row is the gate that
    // fails closed instead of shipping a malformed fixture.
    for (const auto &fixture : kVendoredFiles) {
        QFile file(vendoredPath(fixture.fileName));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        const QByteArray bytes = file.readAll();
        QVERIFY2(bytes.size() >= 12, qPrintable(file.fileName()));

        const quint32 numTables =
            (quint32(quint8(bytes.at(4))) << 8) | quint8(bytes.at(5));
        QVERIFY2(numTables > 0, qPrintable(file.fileName()));

        bool sawHead = false;
        for (quint32 index = 0; index < numTables; ++index) {
            const char *record = bytes.constData() + 12 + 16 * index;
            const QByteArray tag(record, 4);
            const quint32 stored = be32(record + 4);
            const quint32 offset = be32(record + 8);
            const quint32 length = be32(record + 12);
            QVERIFY2(qsizetype(offset) + qsizetype(length) <= bytes.size(),
                     qPrintable(QStringLiteral("%1 table %2 exceeds the file")
                                    .arg(QString::fromLatin1(fixture.fileName),
                                         QString::fromLatin1(tag))));
            QByteArray payload = bytes.mid(qsizetype(offset), qsizetype(length));
            if (tag == QByteArrayLiteral("head")) {
                sawHead = true;
                // OpenType: the adjustment field counts as zero for the head
                // table's own checksum.
                char *data = payload.data();
                data[8] = data[9] = data[10] = data[11] = '\0';
            }
            const quint32 calculated = sfntChecksum(payload.constData(), payload.size());
            QVERIFY2(calculated == stored,
                     qPrintable(QStringLiteral("%1 table %2 checksum stored 0x%3 "
                                               "calculated 0x%4")
                                    .arg(QString::fromLatin1(fixture.fileName),
                                         QString::fromLatin1(tag))
                                    .arg(stored, 8, 16, QLatin1Char('0'))
                                    .arg(calculated, 8, 16, QLatin1Char('0'))));
        }
        QVERIFY2(sawHead, qPrintable(QString::fromLatin1(fixture.fileName)));

        const quint32 whole = sfntChecksum(bytes.constData(), bytes.size());
        QVERIFY2(whole == kWholeFontChecksum,
                 qPrintable(QStringLiteral("%1 whole-font checksum 0x%2 expected 0x%3")
                                .arg(QString::fromLatin1(fixture.fileName))
                                .arg(whole, 8, 16, QLatin1Char('0'))
                                .arg(kWholeFontChecksum, 8, 16, QLatin1Char('0'))));
    }
}

QTEST_MAIN(ControlsFontPinningTests)
#include "tst_controls_font_pinning.moc"
