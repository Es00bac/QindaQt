// SPDX-License-Identifier: LGPL-3.0-or-later
// ADR-0286: the pure per-display / per-desktop wallpaper value — scope
// precedence and fallbacks, edits and bounds, the strict Settings1 codec, and
// the additive schema key that keeps an existing single wallpaper working.
#include <qindaqt/services/wallpaper_assignments/wallpaper_assignments.h>

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/settings/settings_schema.h"

#include <QtTest>

using namespace QindaQt::Services::WallpaperAssignments;

namespace {

const QString kEverywhere = QStringLiteral("qindaqt:jade-fold");
const QString kLeft = QStringLiteral("edid:0123456789abcdef0123456789abcdef");
const QString kRight = QStringLiteral("conn:DP-2");
const QString kWork = QStringLiteral("5b0f0c39-1c49-4a2b-9e44-3b5a0a8e7d11");
const QString kPlay = QStringLiteral("0d2a9a1e-7b0e-4f39-8a61-f3e1d7f9b202");

QVariantMap record(const QString &display, const QString &desktop, const QString &wallpaper)
{
  return {{QStringLiteral("display"), display},
          {QStringLiteral("desktop"), desktop},
          {QStringLiteral("wallpaper"), wallpaper}};
}

QVariantMap document(const QVariantList &records, const QVariant &version = qint64(1))
{
  return {{QStringLiteral("version"), version}, {QStringLiteral("assignments"), records}};
}

WallpaperAssignments sample()
{
  WallpaperAssignments value;
  (void)value.set(kLeft, QString(), QStringLiteral("qindaqt:neon-harbor"));
  (void)value.set(kLeft, kWork, QStringLiteral("/home/user/Pictures/Wallpapers/desk.png"));
  (void)value.set(QString(), kPlay, QStringLiteral("qindaqt:aurora-plain"));
  (void)value.set(kRight, kPlay, QString());
  return value;
}

} // namespace

class WallpaperAssignmentsTests final : public QObject {
  Q_OBJECT

private slots:
  void resolutionPrefersTheMostSpecificChoice();
  void unknownDisplaysAndDesktopsFallBackWithoutLosingChoices();
  void editsValidateAndKeepOneCanonicalOrder();
  void codecRoundTripsAndDefaultsToNoChoices();
  void codecRejectsHostileValuesAsAWhole();
  void largestAdmissibleValueFitsSettings1();
  void schemaDeclaresAnAdditiveObjectKey();
};

void WallpaperAssignmentsTests::resolutionPrefersTheMostSpecificChoice()
{
  const WallpaperAssignments value = sample();
  // Display and desktop together outrank the display's own choice.
  QCOMPARE(value.resolve(kEverywhere, kLeft, kWork),
           (WallpaperResolution{QStringLiteral("/home/user/Pictures/Wallpapers/desk.png"),
                                ResolvedScope::DisplayDesktop}));
  // The display's choice outranks a desktop's choice for every display, so a
  // portrait monitor keeps its picture when the desktop has a landscape one.
  QCOMPARE(value.resolve(kEverywhere, kLeft, kPlay),
           (WallpaperResolution{QStringLiteral("qindaqt:neon-harbor"), ResolvedScope::Display}));
  // A display without its own choice shows the desktop's choice.
  QCOMPARE(value.resolve(kEverywhere, QStringLiteral("edid:ffff"), kPlay),
           (WallpaperResolution{QStringLiteral("qindaqt:aurora-plain"), ResolvedScope::Desktop}));
  // An explicit empty choice is "no wallpaper", not "inherit".
  QCOMPARE(value.resolve(kEverywhere, kRight, kPlay),
           (WallpaperResolution{QString(), ResolvedScope::DisplayDesktop}));
  QCOMPARE(value.resolve(kEverywhere, kRight, kWork),
           (WallpaperResolution{kEverywhere, ResolvedScope::Everywhere}));
  // No choices at all is today's behaviour: one wallpaper everywhere.
  QCOMPARE(WallpaperAssignments{}.resolve(kEverywhere, kLeft, kWork),
           (WallpaperResolution{kEverywhere, ResolvedScope::Everywhere}));
  QCOMPARE(WallpaperAssignments{}.resolve(QString(), kLeft, kWork),
           (WallpaperResolution{QString(), ResolvedScope::Everywhere}));
}

void WallpaperAssignmentsTests::unknownDisplaysAndDesktopsFallBackWithoutLosingChoices()
{
  const WallpaperAssignments value = sample();
  // Identity not known yet (Display1 silent, ambiguous twins): the desktop's
  // choice still applies, then the everywhere wallpaper.
  QCOMPARE(value.resolve(kEverywhere, QString(), kPlay).scope, ResolvedScope::Desktop);
  QCOMPARE(value.resolve(kEverywhere, QString(), kWork),
           (WallpaperResolution{kEverywhere, ResolvedScope::Everywhere}));
  // Current desktop unknown or removed: the display's choice applies.
  QCOMPARE(value.resolve(kEverywhere, kLeft, QString()).scope, ResolvedScope::Display);
  QCOMPARE(value.resolve(kEverywhere, kLeft, QStringLiteral("removed-desktop")).scope,
           ResolvedScope::Display);
  // An unplugged display is simply not asked about; its choices stay saved
  // and come back unchanged when the same stable id reappears.
  QCOMPARE(value.resolve(kEverywhere, QStringLiteral("edid:beef"), QString()).scope,
           ResolvedScope::Everywhere);
  QCOMPARE(value.size(), 4);
  QCOMPARE(value.resolve(kEverywhere, kLeft, kWork).scope, ResolvedScope::DisplayDesktop);
}

void WallpaperAssignmentsTests::editsValidateAndKeepOneCanonicalOrder()
{
  WallpaperAssignments forward;
  QCOMPARE(forward.set(kRight, QString(), QStringLiteral("/a.png")), WallpaperEditError::None);
  QCOMPARE(forward.set(kLeft, kWork, QStringLiteral("/b.png")), WallpaperEditError::None);
  QCOMPARE(forward.set(QString(), kWork, QStringLiteral("/c.png")), WallpaperEditError::None);
  WallpaperAssignments backward;
  QCOMPARE(backward.set(QString(), kWork, QStringLiteral("/c.png")), WallpaperEditError::None);
  QCOMPARE(backward.set(kLeft, kWork, QStringLiteral("/b.png")), WallpaperEditError::None);
  QCOMPARE(backward.set(kRight, QString(), QStringLiteral("/a.png")), WallpaperEditError::None);
  QCOMPARE(forward, backward);
  QCOMPARE(WallpaperAssignments::encodeSettingsValue(forward),
           WallpaperAssignments::encodeSettingsValue(backward));
  QCOMPARE(forward.assignments().constFirst().display, QString());

  // Replacing a scope keeps one record.
  QCOMPARE(forward.set(kLeft, kWork, QStringLiteral("qindaqt:neon-harbor")),
           WallpaperEditError::None);
  QCOMPARE(forward.size(), 3);
  QCOMPARE(forward.find(kLeft, kWork), std::optional<QString>(QStringLiteral("qindaqt:neon-harbor")));

  // Refusals leave the value untouched.
  const WallpaperAssignments before = forward;
  QCOMPARE(forward.set(QString(), QString(), QStringLiteral("/x.png")),
           WallpaperEditError::InvalidScope);
  QCOMPARE(forward.set(QStringLiteral("edid:has space"), QString(), QStringLiteral("/x.png")),
           WallpaperEditError::InvalidScope);
  QCOMPARE(forward.set(QString(QChar(0x202e)) + kLeft, QString(), QStringLiteral("/x.png")),
           WallpaperEditError::InvalidScope);
  QCOMPARE(forward.set(kLeft, QString(129, QLatin1Char('d')), QStringLiteral("/x.png")),
           WallpaperEditError::InvalidScope);
  QCOMPARE(forward.set(QStringLiteral("edid:") + QString(124, QLatin1Char('a')), QString(),
                       QStringLiteral("/x.png")),
           WallpaperEditError::InvalidScope);
  QCOMPARE(forward.set(kLeft, QString(), QStringLiteral("relative.png")),
           WallpaperEditError::InvalidWallpaper);
  QCOMPARE(forward.set(kLeft, QString(), QStringLiteral("qindaqt:")),
           WallpaperEditError::InvalidWallpaper);
  QCOMPARE(forward.set(kLeft, QString(), QStringLiteral("qindaqt:a/b")),
           WallpaperEditError::InvalidWallpaper);
  QCOMPARE(forward.set(kLeft, QString(), QStringLiteral("/a\nb.png")),
           WallpaperEditError::InvalidWallpaper);
  QCOMPARE(forward.set(kLeft, QString(),
                       QLatin1Char('/') + QString(Bounds::maxWallpaperUtf8Bytes, QLatin1Char('w'))),
           WallpaperEditError::InvalidWallpaper);
  QCOMPARE(forward, before);

  // The ceiling admits replacements but no new scope.
  WallpaperAssignments full;
  for (qsizetype index = 0; index < Bounds::maxAssignments; ++index) {
    QCOMPARE(full.set(QStringLiteral("conn:DP-%1").arg(index), QString(), QStringLiteral("/p.png")),
             WallpaperEditError::None);
  }
  QCOMPARE(full.set(QStringLiteral("conn:DP-0"), QString(), QStringLiteral("/q.png")),
           WallpaperEditError::None);
  QCOMPARE(full.set(QStringLiteral("conn:DP-X"), QString(), QStringLiteral("/q.png")),
           WallpaperEditError::Full);
  QCOMPARE(full.size(), Bounds::maxAssignments);

  QVERIFY(forward.remove(kLeft, kWork));
  QVERIFY(!forward.remove(kLeft, kWork));
  QCOMPARE(forward.size(), 2);
}

void WallpaperAssignmentsTests::codecRoundTripsAndDefaultsToNoChoices()
{
  // Absent (older Settings1 peers) and the schema default mean no choices:
  // a saved single wallpaper keeps applying everywhere with no migration.
  for (const QVariant &empty : {QVariant(), QVariant(QVariantMap{})}) {
    const auto decoded = WallpaperAssignments::decodeSettingsValue(empty);
    QVERIFY(decoded.ok());
    QVERIFY(decoded.value->isEmpty());
  }
  QCOMPARE(WallpaperAssignments::encodeSettingsValue(WallpaperAssignments{}), QVariantMap{});

  const WallpaperAssignments value = sample();
  const QVariantMap encoded = WallpaperAssignments::encodeSettingsValue(value);
  QCOMPARE(encoded.value(QStringLiteral("version")).metaType().id(), QMetaType::LongLong);
  QCOMPARE(encoded.value(QStringLiteral("assignments")).toList().size(), 4);
  const auto decoded = WallpaperAssignments::decodeSettingsValue(encoded);
  QVERIFY2(decoded.ok(), qPrintable(decoded.error));
  QCOMPARE(*decoded.value, value);

  // Any record order and any integral version spelling decode to the same
  // canonical value (D-Bus and JSON ingress differ in numeric types).
  const QVariantMap shuffled = document(
      {record(kRight, kPlay, QString()),
       record(QString(), kPlay, QStringLiteral("qindaqt:aurora-plain")),
       record(kLeft, kWork, QStringLiteral("/home/user/Pictures/Wallpapers/desk.png")),
       record(kLeft, QString(), QStringLiteral("qindaqt:neon-harbor"))},
      1.0);
  const auto reordered = WallpaperAssignments::decodeSettingsValue(shuffled);
  QVERIFY2(reordered.ok(), qPrintable(reordered.error));
  QCOMPARE(*reordered.value, value);
  QCOMPARE(WallpaperAssignments::encodeSettingsValue(*reordered.value), encoded);
}

void WallpaperAssignmentsTests::codecRejectsHostileValuesAsAWhole()
{
  const QVariantMap good = record(kLeft, QString(), QStringLiteral("/a.png"));
  const QList<QVariant> hostile{
      QVariant(QStringLiteral("not an object")),
      QVariant(QVariantList{good}),
      document({good}, qint64(2)),
      document({good}, QStringLiteral("1")),
      document({good}, 1.5),
      QVariantMap{{QStringLiteral("assignments"), QVariantList{good}}},
      QVariantMap{{QStringLiteral("version"), qint64(1)},
                  {QStringLiteral("assignments"), QVariantList{good}},
                  {QStringLiteral("extra"), true}},
      QVariantMap{{QStringLiteral("version"), qint64(1)},
                  {QStringLiteral("assignments"), QStringLiteral("x")}},
      document({good, good}),
      document({record(QString(), QString(), QStringLiteral("/a.png"))}),
      document({record(kLeft, QString(), QStringLiteral("relative.png"))}),
      document({record(QStringLiteral("edid:a b"), QString(), QStringLiteral("/a.png"))}),
      document({QVariantMap{{QStringLiteral("display"), kLeft},
                            {QStringLiteral("wallpaper"), QStringLiteral("/a.png")}}}),
      document({QVariantMap{{QStringLiteral("display"), kLeft},
                            {QStringLiteral("desktop"), qint64(3)},
                            {QStringLiteral("wallpaper"), QStringLiteral("/a.png")}}}),
      document({QVariant(QStringLiteral("record"))}),
  };
  for (qsizetype index = 0; index < hostile.size(); ++index) {
    const auto decoded = WallpaperAssignments::decodeSettingsValue(hostile.at(index));
    QVERIFY2(!decoded.ok(), qPrintable(QStringLiteral("hostile value %1 decoded").arg(index)));
    QVERIFY(!decoded.error.isEmpty());
  }

  QVariantList tooMany;
  for (qsizetype index = 0; index <= Bounds::maxAssignments; ++index)
    tooMany.append(record(QStringLiteral("conn:DP-%1").arg(index), QString(), QStringLiteral("/a.png")));
  QVERIFY(!WallpaperAssignments::decodeSettingsValue(document(tooMany)).ok());
  tooMany.removeLast();
  QVERIFY(WallpaperAssignments::decodeSettingsValue(document(tooMany)).ok());
}

void WallpaperAssignmentsTests::largestAdmissibleValueFitsSettings1()
{
  // AGENT-GUARD companion for Bounds::maxWallpaperUtf8Bytes: every record at
  // every bound, desktop ids made of three-byte characters.
  QList<WallpaperAssignment> records;
  for (qsizetype index = 0; index < Bounds::maxAssignments; ++index) {
    const QString suffix = QStringLiteral("%1").arg(index, 2, 10, QLatin1Char('0'));
    QString display = QStringLiteral("edid:") + suffix;
    display += QString(Bounds::maxDisplayIdUtf8Bytes - display.size(), QLatin1Char('a'));
    QString desktop = QString(Bounds::maxDesktopIdLength - suffix.size(), QChar(0x4e00));
    desktop += suffix;
    QString wallpaper = QStringLiteral("/") + suffix;
    wallpaper += QString(Bounds::maxWallpaperUtf8Bytes - wallpaper.size(), QLatin1Char('w'));
    records.append(WallpaperAssignment{display, desktop, wallpaper});
  }
  const auto largest = WallpaperAssignments::fromAssignments(records);
  QVERIFY(largest.has_value());
  const QVariantMap encoded = WallpaperAssignments::encodeSettingsValue(*largest);
  QString error;
  QVERIFY2(QindaQt::Services::SettingsProtocol::BoundedSettingsValueCodec::validateValue(
               encoded, &error),
           qPrintable(error));
}

void WallpaperAssignmentsTests::schemaDeclaresAnAdditiveObjectKey()
{
  QString error;
  const auto schema = QindaQt::Settings::SettingsSchema::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
  QVERIFY2(schema.has_value(), qPrintable(error));
  const auto *definition = schema->definition(QLatin1StringView(SettingsKey));
  QVERIFY(definition != nullptr);
  QVERIFY(definition->type == QindaQt::Settings::SettingValueType::Object);
  // The default is "no choices", so an existing user file that only names
  // appearance.wallpaper keeps showing it on every display (ADR-0280's
  // additive-key rule: no migration writes, no schema version change).
  QCOMPARE(definition->defaultValue.toMap(), QVariantMap{});
  QVERIFY(WallpaperAssignments::decodeSettingsValue(definition->defaultValue).value->isEmpty());
  QVERIFY(schema->validateValue(QLatin1StringView(SettingsKey),
                                WallpaperAssignments::encodeSettingsValue(sample()))
              .isValid());
}

QTEST_GUILESS_MAIN(WallpaperAssignmentsTests)
#include "tst_wallpaper_assignments.moc"
