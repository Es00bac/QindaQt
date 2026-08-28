// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_model/clipboard_media.h>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include "support/clipboard_test_data.h"

#include <QtTest>

using namespace QindaQt::Services::ClipboardModel;

namespace {

QString accepted(const QString &raw)
{
    const MediaCanonicalization canonical = canonicalizeMediaType(raw);
    if (!canonical.accepted()) {
        return QStringLiteral("<rejected>");
    }
    return canonical.canonical;
}

} // namespace

class ClipboardMediaTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void canonicalizesProducerSpellings();
    void refusesNonCanonicalizableShapes();
    void classificationIsAllowlistBased();
    void refusesSensitiveOneTimeAndUnknownClasses();
    void sanitizesSourceLabels();
    void protocolLimitsValidate();
};

void ClipboardMediaTests::canonicalizesProducerSpellings()
{
    QCOMPARE(accepted(ClipboardTest::textFormat()), QStringLiteral("text/plain"));
    QCOMPARE(accepted(QStringLiteral("TEXT/PLAIN")), QStringLiteral("text/plain"));
    QCOMPARE(accepted(QStringLiteral("  Text/Plain  ")), QStringLiteral("text/plain"));
    QCOMPARE(accepted(QStringLiteral("Image/PNG")), QStringLiteral("image/png"));
    QCOMPARE(accepted(ClipboardTest::sensitiveMarker()),
             QStringLiteral("x-kde-passwordmanagerhint"));
    QCOMPARE(accepted(QStringLiteral("application/vnd.vendor+json")),
             QStringLiteral("application/vnd.vendor+json"));
    // Canonical spellings are recognized as canonical by the fast path too.
    QVERIFY(isCanonicalMediaType(QStringLiteral("text/plain")));
    QVERIFY(!isCanonicalMediaType(QStringLiteral("TEXT/PLAIN")));
    QVERIFY(!isCanonicalMediaType(QStringLiteral(" text/plain ")));
}

void ClipboardMediaTests::refusesNonCanonicalizableShapes()
{
    QCOMPARE(accepted(QString()), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("   ")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("text/plain;charset=utf-8")),
             QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("text/plain ")), QStringLiteral("text/plain"));
    QCOMPARE(accepted(QStringLiteral("*/plain")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("text/*")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("a/b/c")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("/plain")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("text/")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("text/plain extra")), QStringLiteral("<rejected>"));
    QCOMPARE(accepted(QStringLiteral("tëxt/plain")), QStringLiteral("<rejected>"));
    QString oversized = QStringLiteral("text/");
    oversized += QString(kMaxMediaTypeLength, QLatin1Char('x'));
    QCOMPARE(accepted(oversized), QStringLiteral("<rejected>"));
}

void ClipboardMediaTests::classificationIsAllowlistBased()
{
    QCOMPARE(classifyMediaType(QStringLiteral("text/plain")), MediaClass::Storable);
    QCOMPARE(classifyMediaType(QStringLiteral("text/html")), MediaClass::Storable);
    QCOMPARE(classifyMediaType(QStringLiteral("text/uri-list")), MediaClass::Storable);
    QCOMPARE(classifyMediaType(QStringLiteral("image/png")), MediaClass::Storable);
    QCOMPARE(classifyMediaType(QStringLiteral("image/jpeg")), MediaClass::Storable);
    QCOMPARE(classifyMediaType(ClipboardTest::sensitiveMarker()), MediaClass::Sensitive);
    QCOMPARE(classifyMediaType(ClipboardTest::qindaqtSecret()), MediaClass::Sensitive);
    QCOMPARE(classifyMediaType(ClipboardTest::oneTimeMarker()), MediaClass::OneTime);
    // Unknown vendor types are non-storable by default: fail closed.
    QCOMPARE(classifyMediaType(ClipboardTest::unknownMediaType()), MediaClass::NonStorable);
    // Non-canonical input must classify as non-storable, never storable.
    QCOMPARE(classifyMediaType(QStringLiteral("TEXT/PLAIN")), MediaClass::NonStorable);
    QCOMPARE(classifyMediaType(QString()), MediaClass::NonStorable);
}

void ClipboardMediaTests::refusesSensitiveOneTimeAndUnknownClasses()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);
    QCOMPARE(model.generation(), quint32 { 1 });

    const auto refuse = [&model](const ClipboardValue &value) {
        return model.admit(value, ClipboardTest::kGen, QStringLiteral("fixture-source"),
                           1)
            .error;
    };

    QCOMPARE(refuse(ClipboardTest::valueWithMedia(ClipboardTest::sensitiveMarker(),
                                                  QStringLiteral("fixture"))),
             ClipboardError::SensitiveRefused);
    QCOMPARE(refuse(ClipboardTest::valueWithMedia(ClipboardTest::qindaqtSecret(),
                                                  QStringLiteral("fixture"))),
             ClipboardError::SensitiveRefused);
    // Sensitive outranks one-time and storable members of the same value.
    ClipboardValue mixed = ClipboardTest::fixtureAlpha();
    mixed.formats.append({ ClipboardTest::sensitiveMarker(), QByteArrayLiteral("1") });
    QCOMPARE(refuse(mixed), ClipboardError::SensitiveRefused);

    QCOMPARE(refuse(ClipboardTest::valueWithMedia(ClipboardTest::oneTimeMarker(),
                                                  QStringLiteral("fixture"))),
             ClipboardError::OneTimeRefused);

    QCOMPARE(refuse(ClipboardTest::valueWithMedia(ClipboardTest::unknownMediaType(),
                                                  QStringLiteral("fixture"))),
             ClipboardError::NonStorableRefused);
    QCOMPARE(refuse(ClipboardTest::valueWithMedia(QStringLiteral("text/plain;charset=utf-8"),
                                                  QStringLiteral("fixture"))),
             ClipboardError::MediaTypeRejected);
    // Nothing leaked into the history across all refusals.
    QCOMPARE(model.snapshot().entries.size(), 0);
    QCOMPARE(model.revision(), quint64 { 0 });
}

void ClipboardMediaTests::sanitizesSourceLabels()
{
    QCOMPARE(sanitizeSourceLabel(QStringLiteral("fixture-app"), 64),
             QStringLiteral("fixture-app"));
    // Newlines, tabs, and format characters become spaces; nothing passes
    // through that could smuggle control sequences into presentation.
    QCOMPARE(sanitizeSourceLabel(QStringLiteral("app\nname\tv1"), 64),
             QStringLiteral("app name v1"));
    QCOMPARE(sanitizeSourceLabel(QString(QChar(0x200e)) + QStringLiteral("bidi"), 64),
             QStringLiteral(" bidi"));
    // Clamping is by code units and deterministic.
    const QString longLabel = QStringLiteral("abcdefghijklmnop");
    QCOMPARE(sanitizeSourceLabel(longLabel, 4), QStringLiteral("abcd"));
    QCOMPARE(sanitizeSourceLabel(QString(), 64), QString());
}

void ClipboardMediaTests::protocolLimitsValidate()
{
    HistoryLimits limits;
    QVERIFY(isValidLimits(limits));

    HistoryLimits narrowed = limits;
    narrowed.maxEntries = 3;
    narrowed.maxPinnedEntries = qMin(narrowed.maxPinnedEntries, narrowed.maxEntries);
    narrowed.maxItemPayloadBytes = 64;
    narrowed.maxTotalPayloadBytes = 256;
    QVERIFY(isValidLimits(narrowed));

    HistoryLimits zeroEntries = limits;
    zeroEntries.maxEntries = 0;
    QVERIFY(!isValidLimits(zeroEntries));

    HistoryLimits tooWide = limits;
    tooWide.maxEntries = kMaxEntries + 1;
    QVERIFY(!isValidLimits(tooWide));

    HistoryLimits badPins = limits;
    badPins.maxPinnedEntries = limits.maxEntries + 1;
    QVERIFY(!isValidLimits(badPins));

    HistoryLimits itemOverTotal = limits;
    itemOverTotal.maxTotalPayloadBytes = limits.maxItemPayloadBytes - 1;
    QVERIFY(!isValidLimits(itemOverTotal));

    HistoryLimits overProtocolBytes = limits;
    overProtocolBytes.maxItemPayloadBytes = kMaxItemPayloadBytes + 1;
    QVERIFY(!isValidLimits(overProtocolBytes));

    HistoryLimits zeroFormats = limits;
    zeroFormats.maxFormatsPerItem = 0;
    QVERIFY(!isValidLimits(zeroFormats));
}

QTEST_MAIN(ClipboardMediaTests)
#include "tst_clipboard_media.moc"
