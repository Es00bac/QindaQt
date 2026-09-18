// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_geometry.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QTest>

using namespace QindaQt::Services::TabletDevices;

namespace {

TabletDeviceSnapshot wacomPen() {
    TabletDeviceSnapshot pen;
    pen.deviceId = QStringLiteral("event19");
    pen.name = QStringLiteral("Wacom One Pen Display 13 Pen");
    pen.deviceGroupId = QStringLiteral("group-wacom");
    pen.vendorId = 1386;
    pen.productId = 934;
    pen.tabletTool = true;
    return pen;
}

TabletOutputCandidate output(const QString &connector, const QString &vendor,
                             const QString &model, bool internalPanel = false) {
    return TabletOutputCandidate{connector, vendor, model, model,
                                 internalPanel, true};
}

} // namespace

class TabletPolicyValuesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void matchesTheDisplayTabletVendorByEitherEdidForm();
    void neverMatchesTheInternalPanel();
    void refusesToGuessBetweenTwoEqualMatches();
    void fallsBackToTheProductNameWhenTheVendorIsUnknown();
    void genericWordsAloneAreNotAMatch();
    void aPadWithoutAPenMapsNothing();
    void ledgerRoundTripsAndDropsMalformedRecords();
    void ledgerRefusesANamedOutputWithoutAName();
    void calibrationSolvesTheAffineFit();
    void calibrationRefusesDegenerateMeasurements();
    void letterboxCentersTheTabletsProportions();
    void pressureCurveValidationMatchesWhatKWinCanRead();
    void theLedgerKeyIsStableAcrossAReplug();
    void theLedgerKeyIsNeverKWinsDeviceGroupId();
    void aLedgerWrittenUnderTheOldPointerHashIsDropped();
};

void TabletPolicyValuesTest::matchesTheDisplayTabletVendorByEitherEdidForm() {
    // KWin publishes the decoded vendor name when the PNP id is known and the
    // raw three-letter code otherwise; both must match.
    const QList<TabletOutputCandidate> decoded{
        output(QStringLiteral("eDP-1"), QStringLiteral("Lenovo Group Limited"),
               QStringLiteral("eDP-1-0x9052"), true),
        output(QStringLiteral("HDMI-A-1"),
               QStringLiteral("Wacom Technology Corp."),
               QStringLiteral("Wacom One 13")),
    };
    const TabletOutputMatch first = matchTabletOutput(wacomPen(), decoded);
    QCOMPARE(first.reason, TabletMatchReason::DisplayTabletVendor);
    QCOMPARE(first.connectorName, QStringLiteral("HDMI-A-1"));

    const QList<TabletOutputCandidate> rawCode{
        output(QStringLiteral("DP-2"), QStringLiteral("WAC"),
               QStringLiteral("One 13")),
    };
    const TabletOutputMatch second = matchTabletOutput(wacomPen(), rawCode);
    QCOMPARE(second.reason, TabletMatchReason::DisplayTabletVendor);
    QCOMPARE(second.connectorName, QStringLiteral("DP-2"));
    QVERIFY(isDisplayTabletVendor(QStringLiteral("XP-PEN")));
    QVERIFY(!isDisplayTabletVendor(QStringLiteral("Dell Inc.")));
}

void TabletPolicyValuesTest::neverMatchesTheInternalPanel() {
    // The defect this feature exists to remove: the pen drawing on the
    // laptop's own screen. An internal panel is never a pen display, even
    // when nothing else is connected.
    const QList<TabletOutputCandidate> outputs{
        output(QStringLiteral("eDP-1"), QStringLiteral("Wacom Technology Corp."),
               QStringLiteral("Wacom One 13"), true),
    };
    QCOMPARE(matchTabletOutput(wacomPen(), outputs).reason,
             TabletMatchReason::NoMatch);
}

void TabletPolicyValuesTest::refusesToGuessBetweenTwoEqualMatches() {
    const QList<TabletOutputCandidate> outputs{
        output(QStringLiteral("HDMI-A-1"), QStringLiteral("WAC"),
               QStringLiteral("One 13")),
        output(QStringLiteral("DP-1"), QStringLiteral("WAC"),
               QStringLiteral("Cintiq 16")),
    };
    const TabletOutputMatch match = matchTabletOutput(wacomPen(), outputs);
    QCOMPARE(match.reason, TabletMatchReason::Ambiguous);
    QVERIFY(match.connectorName.isEmpty());
    QVERIFY(!match.decided());
}

void TabletPolicyValuesTest::fallsBackToTheProductNameWhenTheVendorIsUnknown() {
    const QList<TabletOutputCandidate> outputs{
        output(QStringLiteral("HDMI-A-1"), QStringLiteral("Generic Screens"),
               QStringLiteral("Cintiq 16 HD")),
        output(QStringLiteral("DP-1"), QStringLiteral("Dell Inc."),
               QStringLiteral("U2720Q")),
    };
    TabletDeviceSnapshot pen = wacomPen();
    pen.name = QStringLiteral("Cintiq 16 Pen");
    const TabletOutputMatch match = matchTabletOutput(pen, outputs);
    QCOMPARE(match.reason, TabletMatchReason::ProductName);
    QCOMPARE(match.connectorName, QStringLiteral("HDMI-A-1"));
}

void TabletPolicyValuesTest::genericWordsAloneAreNotAMatch() {
    // "Pen" and "Display" describe every tablet and half the monitors; a
    // match on one of those would map an opaque tablet to a random screen.
    const QList<TabletOutputCandidate> outputs{
        output(QStringLiteral("HDMI-A-1"), QStringLiteral("Generic Screens"),
               QStringLiteral("Pen Display Monitor")),
    };
    TabletDeviceSnapshot pen = wacomPen();
    pen.name = QStringLiteral("Some Pen Display Pen");
    QCOMPARE(matchTabletOutput(pen, outputs).reason,
             TabletMatchReason::NoMatch);
}

void TabletPolicyValuesTest::aPadWithoutAPenMapsNothing() {
    TabletDeviceSnapshot pad = wacomPen();
    pad.tabletTool = false;
    pad.tabletPad = true;
    const QList<TabletOutputCandidate> outputs{
        output(QStringLiteral("HDMI-A-1"), QStringLiteral("WAC"),
               QStringLiteral("One 13")),
    };
    QCOMPARE(matchTabletOutput(pad, outputs).reason, TabletMatchReason::NoMatch);
}

void TabletPolicyValuesTest::ledgerRoundTripsAndDropsMalformedRecords() {
    TabletMappingLedger ledger;
    TabletMappingRecord record;
    record.choice = TabletMapChoice::NamedOutput;
    record.outputName = QStringLiteral("HDMI-A-1");
    record.userChosen = true;
    record.announced = true;
    record.deviceName = QStringLiteral("Wacom One Pen Display 13 Pen");
    ledger.setRecord(QStringLiteral("group-wacom"), record);

    const QVariantMap document = ledger.toVariantMap();
    const TabletMappingLedger parsed =
        TabletMappingLedger::fromVariantMap(document);
    QCOMPARE(parsed, ledger);
    QCOMPARE(parsed.record(QStringLiteral("group-wacom")).outputName,
             QStringLiteral("HDMI-A-1"));
    QVERIFY(parsed.record(QStringLiteral("group-wacom")).userChosen);

    // A record this version cannot read must not cost the user the records
    // it can: bad entries are dropped, good ones survive.
    QVariantMap mixed = document;
    mixed.insert(QStringLiteral("group-broken"), QStringLiteral("not a map"));
    mixed.insert(QStringLiteral("group-unknown-choice"),
                 QVariantMap{{QStringLiteral("choice"),
                              QStringLiteral("somewhere-else")}});
    const TabletMappingLedger tolerant =
        TabletMappingLedger::fromVariantMap(mixed);
    QCOMPARE(tolerant.size(), 1);
    QCOMPARE(tolerant.groupIds(), QStringList{QStringLiteral("group-wacom")});
}

void TabletPolicyValuesTest::ledgerRefusesANamedOutputWithoutAName() {
    // Such a record would make the policy write an empty outputName and hand
    // the pen silently back to the active screen.
    const QVariantMap document{
        {QStringLiteral("group-wacom"),
         QVariantMap{{QStringLiteral("choice"), QStringLiteral("output")},
                     {QStringLiteral("userChosen"), true}}}};
    QVERIFY(TabletMappingLedger::fromVariantMap(document).isEmpty());
}

void TabletPolicyValuesTest::calibrationSolvesTheAffineFit() {
    // Measurements that are the targets shrunk by 10% and shifted: the fit
    // must undo exactly that.
    const QList<QPointF> targets{{0.1, 0.1}, {0.9, 0.1}, {0.9, 0.9}, {0.1, 0.9}};
    QList<QPointF> measured;
    for (const QPointF &target : targets) {
        measured.append(QPointF(target.x() * 0.9 + 0.05,
                                target.y() * 0.8 + 0.10));
    }
    const QString matrix = calibrationMatrixFor(measured, targets);
    QVERIFY(!matrix.isEmpty());
    const QStringList values = matrix.split(QLatin1Char(','));
    QCOMPARE(values.size(), 16);
    // Row-major 4x4 with the affine in rows 0 and 1 (KWin's serializeMatrix).
    QVERIFY(qAbs(values.at(0).toDouble() - 1.0 / 0.9) < 1e-6);
    QVERIFY(qAbs(values.at(1).toDouble()) < 1e-6);
    QVERIFY(qAbs(values.at(3).toDouble()) < 1e-9);
    QVERIFY(qAbs(values.at(5).toDouble() - 1.0 / 0.8) < 1e-6);
    QCOMPARE(values.at(10).toDouble(), 1.0);
    QCOMPARE(values.at(15).toDouble(), 1.0);
    // And the round trip: applying the fit to a measurement lands on target.
    const double a = values.at(0).toDouble();
    const double c = values.at(2).toDouble();
    QVERIFY(qAbs(a * measured.at(0).x() + c - targets.at(0).x()) < 1e-6);
}

void TabletPolicyValuesTest::calibrationRefusesDegenerateMeasurements() {
    const QList<QPointF> targets{{0.1, 0.1}, {0.9, 0.1}, {0.9, 0.9}, {0.1, 0.9}};
    // Every measurement on one line: the fit would collapse the tablet.
    const QList<QPointF> collinear{{0.2, 0.2}, {0.4, 0.4}, {0.6, 0.6}, {0.8, 0.8}};
    QVERIFY(calibrationMatrixFor(collinear, targets).isEmpty());
    // Identical points.
    const QList<QPointF> same{{0.5, 0.5}, {0.5, 0.5}, {0.5, 0.5}, {0.5, 0.5}};
    QVERIFY(calibrationMatrixFor(same, targets).isEmpty());
    // Mismatched lengths and too few points.
    QVERIFY(calibrationMatrixFor({{0.1, 0.1}}, targets).isEmpty());
    QVERIFY(identityCalibrationMatrix().split(QLatin1Char(',')).size() == 16);
}

void TabletPolicyValuesTest::letterboxCentersTheTabletsProportions() {
    // A 16:9 screen and a 4:3 tablet: full height, narrower and centered.
    const TabletArea narrow = letterboxArea(4.0 / 3.0, 1920.0, 1080.0);
    QVERIFY(narrow.isValid());
    QVERIFY(qAbs(narrow.height - 1.0) < 1e-9);
    QVERIFY(qAbs(narrow.width - (4.0 / 3.0) / (16.0 / 9.0)) < 1e-9);
    QVERIFY(qAbs(narrow.x - (1.0 - narrow.width) / 2.0) < 1e-9);

    // A wider tablet than screen: full width, shorter and centered.
    const TabletArea wide = letterboxArea(2.5, 1920.0, 1080.0);
    QVERIFY(qAbs(wide.width - 1.0) < 1e-9);
    QVERIFY(wide.height < 1.0);
    QVERIFY(qAbs(wide.y - (1.0 - wide.height) / 2.0) < 1e-9);

    // Equal proportions, or an unknown tablet size, is the whole surface.
    QVERIFY(letterboxArea(16.0 / 9.0, 1920.0, 1080.0).isWhole());
    QVERIFY(letterboxArea(-1.0, 1920.0, 1080.0).isWhole());
}

void TabletPolicyValuesTest::pressureCurveValidationMatchesWhatKWinCanRead() {
    // KWin's own default and a plain threshold curve.
    QVERIFY(isValidPressureCurve(QStringLiteral("0,0;1,1;")));
    QVERIFY(isValidPressureCurve(pressureCurveForThreshold(0.2)));
    // Fewer than two points: KWin keeps its old curve.
    QVERIFY(!isValidPressureCurve(QStringLiteral("0.5,0.5;")));
    // Out of range and non-increasing x.
    QVERIFY(!isValidPressureCurve(QStringLiteral("0,0;1.4,1;")));
    QVERIFY(!isValidPressureCurve(QStringLiteral("0.8,0;0.2,1;")));
    QVERIFY(!isValidPressureCurve(QString()));
    // The threshold is clamped rather than refused.
    QVERIFY(isValidPressureCurve(pressureCurveForThreshold(5.0)));
}

void TabletPolicyValuesTest::theLedgerKeyIsStableAcrossAReplug() {
    // KWin hands out a NEW deviceGroupId on every re-plug: it hashes the
    // libinput device group's pointer address
    // (kwin-6.6.6 src/backends/libinput/device.cpp:450). The identity the
    // ledger is keyed on must not move when that does.
    TabletDeviceSnapshot before = wacomPen();
    before.deviceId = QStringLiteral("event19");
    before.deviceGroupId = QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=");
    before.vendorId = 1386;
    before.productId = 934;

    TabletDeviceSnapshot after = before;
    after.deviceId = QStringLiteral("event23");            // new event node
    after.deviceGroupId = QStringLiteral("Zm9vYmFyYmF6cXV4MTIzNDU2Nzg5MA=="); // new group

    QCOMPARE(tabletIdentity(before), tabletIdentity(after));
    QVERIFY(!tabletIdentity(before).isEmpty());
    // It is KWin's own per-device config key: vendor, product, name
    // (src/backends/libinput/connection.cpp:716).
    QCOMPARE(tabletIdentity(before),
             QStringLiteral("1386:934:Wacom One Pen Display 13"));

    // A pen and its pad are one tablet, without relying on the group id.
    TabletDeviceSnapshot pad = before;
    pad.tabletTool = false;
    pad.tabletPad = true;
    pad.name = QStringLiteral("Wacom One Pen Display 13 Pad");
    pad.deviceGroupId = QStringLiteral("a-different-group-entirely");
    QCOMPARE(tabletIdentity(pad), tabletIdentity(before));

    // A different model does not collide.
    TabletDeviceSnapshot other = before;
    other.productId = 935;
    QVERIFY(tabletIdentity(other) != tabletIdentity(before));

    // Nothing stable to key on yields no identity, so nothing is recorded
    // under a key that could never be found again.
    TabletDeviceSnapshot anonymous;
    anonymous.tabletTool = true;
    QVERIFY(tabletIdentity(anonymous).isEmpty());
}

void TabletPolicyValuesTest::theLedgerKeyIsNeverKWinsDeviceGroupId() {
    // AGENT-GUARD: this row exists to fail if the key ever regresses to
    // `deviceGroupId`. That id is a hash of a pointer address; keying on it
    // silently breaks "the mapping survives a re-plug", which is one of the
    // promises Checkpoint L is defined by.
    TabletDeviceSnapshot pen = wacomPen();
    pen.vendorId = 1386;
    pen.productId = 934;
    pen.deviceGroupId = QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=");

    const QString identity = tabletIdentity(pen);
    QVERIFY2(identity != pen.deviceGroupId,
             "the ledger key must not be KWin's deviceGroupId");
    QVERIFY2(!identity.contains(pen.deviceGroupId),
             "the ledger key must not embed KWin's deviceGroupId");
    // And the shape is the stable one: two separators, vendor first.
    QCOMPARE(identity.count(QLatin1Char(':')), 2);
    QVERIFY(identity.startsWith(QStringLiteral("1386:")));
    QVERIFY(!isLegacyPointerHashIdentity(identity));
}

void TabletPolicyValuesTest::aLedgerWrittenUnderTheOldPointerHashIsDropped() {
    // A record keyed on a pointer hash can never match a live device again.
    // Keeping it would leave a permanent phantom in the ledger and in the
    // Settings list, so the migration drops it and keeps the rest.
    QVERIFY(isLegacyPointerHashIdentity(
        QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=")));
    QVERIFY(!isLegacyPointerHashIdentity(
        QStringLiteral("1386:934:Wacom One Pen Display 13")));

    const QVariantMap document{
        {QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc="),
         QVariantMap{{QStringLiteral("choice"), QStringLiteral("output")},
                     {QStringLiteral("outputName"), QStringLiteral("HDMI-A-1")},
                     {QStringLiteral("userChosen"), true}}},
        {QStringLiteral("1386:934:Wacom One Pen Display 13"),
         QVariantMap{{QStringLiteral("choice"), QStringLiteral("output")},
                     {QStringLiteral("outputName"), QStringLiteral("DP-2")},
                     {QStringLiteral("userChosen"), true}}},
    };
    const TabletMappingLedger ledger =
        TabletMappingLedger::fromVariantMap(document);
    QCOMPARE(ledger.size(), 1);
    QCOMPARE(ledger.groupIds(),
             QStringList{QStringLiteral("1386:934:Wacom One Pen Display 13")});
    QCOMPARE(ledger.record(QStringLiteral("1386:934:Wacom One Pen Display 13"))
                 .outputName,
             QStringLiteral("DP-2"));
}

QTEST_APPLESS_MAIN(TabletPolicyValuesTest)
#include "tst_tablet_policy_values.moc"
