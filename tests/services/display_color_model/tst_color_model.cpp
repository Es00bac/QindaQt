// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>
#include <limits>
#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_model.h>
#include <qindaqt/services/display_color_model/color_validation.h>
#include "support/color_test_data.h"

using namespace QindaQt::DisplayColor;
using namespace QindaQt::DisplayColor::Testing;

class ColorModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInitialModelAndEpoch();
    void testCatalogIngestionAndResolution();
    void testOutputCapabilityValidation();
    void testAssignmentAndStateEvaluation();
    void testDegradedHdrUnsupported();
    void testDegradedWcgUnsupported();
    void testDegradedProfileNotFoundAndLkgFallback();
    void testLineageValidationAndRejection();
    void testDeterministicLineageFingerprint();
    void testOutputLifecycleRemoval();
    void testEpochReset();
    void testHostileInputsRejectedAtomically();
    void testMaxOutputsAggregateCap();
};

void ColorModelTest::testInitialModelAndEpoch()
{
    ColorModel model("epoch-test-1");
    QCOMPARE(model.serviceEpoch(), QString("epoch-test-1"));
    QCOMPARE(model.revision(), 0u);

    const auto snap = model.snapshot();
    QCOMPARE(snap.serviceEpoch, QString("epoch-test-1"));
    QCOMPARE(snap.revision, 0u);
    QVERIFY(snap.catalog.profiles.isEmpty());
    QVERIFY(snap.outputs.isEmpty());
    QVERIFY(!snap.lineageFingerprint.isEmpty());
}

void ColorModelTest::testCatalogIngestionAndResolution()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::BuiltIn);
    const auto pDci = createSampleProfile("dci-p3", "Display P3", ProfileOrigin::System, ColorSpaceGamut::DciP3);

    QVERIFY(model.setCatalog({pSrgb, pDci}, "srgb-std"));
    QCOMPARE(model.revision(), 1u);

    const auto found = model.findProfile("srgb-std");
    QVERIFY(found.has_value());
    QCOMPARE(found->displayName, QString("Standard sRGB"));

    const auto notFound = model.findProfile("non-existent");
    QVERIFY(!notFound.has_value());
}

void ColorModelTest::testOutputCapabilityValidation()
{
    ColorModel model("epoch-1");

    // Invalid luminance bounds
    {
        OutputColorCapabilities badCaps;
        badCaps.stableId = "DP-1";
        badCaps.minLuminanceNits = 500.0;
        badCaps.maxLuminanceNits = 300.0; // min > max
        QVERIFY(!model.updateCapabilities(badCaps));
    }

    // Invalid HDR transfer function omission
    {
        OutputColorCapabilities badHdr;
        badHdr.stableId = "HDMI-1";
        badHdr.supportsHdr = true;
        badHdr.supportedTransferFunctions = {TransferFunction::Srgb}; // No PQ or HLG
        QVERIFY(!model.updateCapabilities(badHdr));
    }

    // Valid capabilities
    const auto validCaps = createSampleCapabilities("DP-1", true, true);
    QVERIFY(model.updateCapabilities(validCaps));
    QCOMPARE(model.revision(), 1u);

    const auto storedCaps = model.capabilities("DP-1");
    QVERIFY(storedCaps.has_value());
    QCOMPARE(storedCaps->stableId, QString("DP-1"));
    QVERIFY(storedCaps->supportsHdr);
}

void ColorModelTest::testAssignmentAndStateEvaluation()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::BuiltIn);
    model.setCatalog({pSrgb}, "srgb-std");

    const auto caps = createSampleCapabilities("DP-1", true, true);
    model.updateCapabilities(caps);

    OutputColorAssignment assignment;
    assignment.stableId = "DP-1";
    assignment.profileId = "srgb-std";
    assignment.policy = OutputColorPolicy::SdrSrgb;
    assignment.intent = RenderingIntent::RelativeColorimetric;

    QVERIFY(model.requestAssignment(assignment));

    const auto state = model.outputState("DP-1");
    QVERIFY(state.has_value());
    QCOMPARE(state->stableId, QString("DP-1"));
    QVERIFY(!state->isDegraded);
    QCOMPARE(state->degradedReason, DegradedReason::None);
    QCOMPARE(state->activeProfileId, QString("srgb-std"));
    QCOMPARE(state->appliedAssignment.policy, OutputColorPolicy::SdrSrgb);
    QCOMPARE(state->appliedAssignment.intent, RenderingIntent::RelativeColorimetric);
}

void ColorModelTest::testDegradedHdrUnsupported()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB");
    const auto pHdr = createSampleProfile("hdr-pq", "HDR PQ", ProfileOrigin::System,
                                          ColorSpaceGamut::Bt2020);
    model.setCatalog({pSrgb, pHdr}, "srgb-std");

    // Output only supports SDR
    const auto sdrCaps = createSampleCapabilities("eDP-1", false, false);
    model.updateCapabilities(sdrCaps);

    // Request HDR with a distinct HDR profile
    OutputColorAssignment req;
    req.stableId = "eDP-1";
    req.profileId = "hdr-pq";
    req.policy = OutputColorPolicy::HdrEnabled;

    QVERIFY(model.requestAssignment(req));

    const auto state = model.outputState("eDP-1");
    QVERIFY(state.has_value());
    QVERIFY(state->isDegraded);
    QCOMPARE(state->degradedReason, DegradedReason::HdrUnsupported);
    // Truthfully clamped to SDR
    QCOMPARE(state->appliedAssignment.policy, OutputColorPolicy::SdrSrgb);
    // AGENT-GUARD: The clamped SDR policy must not keep the requested HDR
    // profile; applied truth falls back to the default sRGB profile.
    QCOMPARE(state->appliedAssignment.profileId, QString("srgb-std"));
    // The requested intent stays truthful for observers
    QCOMPARE(state->requestedAssignment.profileId, QString("hdr-pq"));
    QCOMPARE(state->requestedAssignment.policy, OutputColorPolicy::HdrEnabled);
}

void ColorModelTest::testDegradedWcgUnsupported()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::BuiltIn);
    model.setCatalog({pSrgb}, "srgb-std");

    // Output does not support WCG
    const auto sdrCaps = createSampleCapabilities("HDMI-A-1", false, false);
    model.updateCapabilities(sdrCaps);

    // Request WCG
    OutputColorAssignment req;
    req.stableId = "HDMI-A-1";
    req.profileId = "srgb-std";
    req.policy = OutputColorPolicy::SdrWcg;

    QVERIFY(model.requestAssignment(req));

    const auto state = model.outputState("HDMI-A-1");
    QVERIFY(state.has_value());
    QVERIFY(state->isDegraded);
    QCOMPARE(state->degradedReason, DegradedReason::WcgUnsupported);
    QCOMPARE(state->appliedAssignment.policy, OutputColorPolicy::SdrSrgb);
}

void ColorModelTest::testDegradedProfileNotFoundAndLkgFallback()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::BuiltIn);
    const auto pValidCustom = createSampleProfile("custom-calib", "Calibrated Profile", ProfileOrigin::UserImported);
    model.setCatalog({pSrgb, pValidCustom}, "srgb-std");

    const auto caps = createSampleCapabilities("DP-2", true, true);
    model.updateCapabilities(caps);

    // 1. Establish valid custom profile as LKG
    OutputColorAssignment validReq;
    validReq.stableId = "DP-2";
    validReq.profileId = "custom-calib";
    model.requestAssignment(validReq);

    auto state = model.outputState("DP-2");
    QVERIFY(state.has_value());
    QVERIFY(!state->isDegraded);
    QCOMPARE(state->activeProfileId, QString("custom-calib"));

    // 2. Request a non-existent profile
    OutputColorAssignment missingReq;
    missingReq.stableId = "DP-2";
    missingReq.profileId = "deleted-or-missing-profile";
    model.requestAssignment(missingReq);

    state = model.outputState("DP-2");
    QVERIFY(state.has_value());
    QVERIFY(state->isDegraded);
    QCOMPARE(state->degradedReason, DegradedReason::ProfileNotFound);
    // AGENT-GUARD: Confirms fallback to LKG "custom-calib" rather than crashing or applying invalid ID
    QCOMPARE(state->activeProfileId, QString("custom-calib"));
}

void ColorModelTest::testLineageValidationAndRejection()
{
    ColorModel model("epoch-auth-42");

    // Matching epoch and the exact current revision is accepted
    QVERIFY(model.validateLineage("epoch-auth-42", model.revision()));

    // AGENT-GUARD: A newer revision can only be published by the model
    // itself; a foreign claim to one is an out-of-order publication and is
    // rejected fail-closed exactly like a stale one.
    QVERIFY(!model.validateLineage("epoch-auth-42", model.revision() + 10));

    // Mismatched epoch fails closed even with the exact revision
    QVERIFY(!model.validateLineage("foreign-epoch-99", model.revision()));

    // Stale revision (less than current revision after mutations)
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB");
    model.setCatalog({pSrgb});
    QVERIFY(!model.validateLineage("epoch-auth-42", 0)); // current revision is 1
    QVERIFY(model.validateLineage("epoch-auth-42", 1));

    // Epoch reset changes the epoch; prior-epoch lineage is never ordered
    // against the new one even at revision 0.
    model.resetEpoch("epoch-reborn");
    QVERIFY(!model.validateLineage("epoch-auth-42", 0));
    QVERIFY(model.validateLineage("epoch-reborn", 0));
}

void ColorModelTest::testDeterministicLineageFingerprint()
{
    ColorModel modelA("epoch-fixed");
    ColorModel modelB("epoch-fixed");

    const auto p1 = createSampleProfile("p1", "Profile 1");
    const auto p2 = createSampleProfile("p2", "Profile 2");

    modelA.setCatalog({p1, p2});
    modelB.setCatalog({p2, p1}); // reverse order in input

    const auto caps = createSampleCapabilities("DP-1");
    modelA.updateCapabilities(caps);
    modelB.updateCapabilities(caps);

    const auto snapA = modelA.snapshot();
    const auto snapB = modelB.snapshot();

    QCOMPARE(snapA.lineageFingerprint, snapB.lineageFingerprint);
}

void ColorModelTest::testOutputLifecycleRemoval()
{
    ColorModel model("epoch-1");
    const auto caps = createSampleCapabilities("DP-1");
    model.updateCapabilities(caps);
    QVERIFY(model.capabilities("DP-1").has_value());

    QVERIFY(model.removeOutput("DP-1"));
    QVERIFY(!model.capabilities("DP-1").has_value());
    QVERIFY(!model.outputState("DP-1").has_value());
}

void ColorModelTest::testEpochReset()
{
    ColorModel model("epoch-old");
    const auto p1 = createSampleProfile("p1", "Profile 1");
    model.setCatalog({p1});
    QCOMPARE(model.revision(), 1u);

    model.resetEpoch("epoch-new");
    QCOMPARE(model.serviceEpoch(), QString("epoch-new"));
    QCOMPARE(model.revision(), 0u);
}

void ColorModelTest::testHostileInputsRejectedAtomically()
{
    ColorModel model("epoch-1");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB");
    model.setCatalog({pSrgb}, "srgb-std");
    const auto caps = createSampleCapabilities("DP-1", true, true);
    QVERIFY(model.updateCapabilities(caps));
    const auto before = model.snapshot();
    QCOMPARE(model.revision(), 2u);

    // NaN luminance passes ordered comparisons but must fail closed
    {
        OutputColorCapabilities nanCaps = caps;
        nanCaps.minLuminanceNits = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!model.updateCapabilities(nanCaps));
    }

    // Infinite peak luminance must fail closed
    {
        OutputColorCapabilities infCaps = caps;
        infCaps.maxLuminanceNits = std::numeric_limits<double>::infinity();
        QVERIFY(!model.updateCapabilities(infCaps));
    }

    // Hostile enum casts in capability lists must fail closed
    {
        OutputColorCapabilities enumCaps = caps;
        enumCaps.supportedGamuts = {static_cast<ColorSpaceGamut>(99u)};
        QVERIFY(!model.updateCapabilities(enumCaps));
    }
    {
        OutputColorCapabilities enumCaps = caps;
        enumCaps.supportedTransferFunctions = {static_cast<TransferFunction>(0xDEADBEEFu)};
        QVERIFY(!model.updateCapabilities(enumCaps));
    }

    // Oversized capability lists must fail closed
    {
        OutputColorCapabilities bigCaps = caps;
        bigCaps.supportedGamuts = QList<ColorSpaceGamut>(16, ColorSpaceGamut::Srgb);
        QVERIFY(!model.updateCapabilities(bigCaps));
    }

    // Hostile enum casts in assignments must fail closed
    {
        OutputColorAssignment badPolicy = createSampleAssignment("DP-1");
        badPolicy.policy = static_cast<OutputColorPolicy>(77u);
        QVERIFY(!model.requestAssignment(badPolicy));
    }
    {
        OutputColorAssignment badIntent = createSampleAssignment("DP-1");
        badIntent.intent = static_cast<RenderingIntent>(99u);
        QVERIFY(!model.requestAssignment(badIntent));
    }

    // NaN SDR brightness must fail closed
    {
        OutputColorAssignment nanBright = createSampleAssignment("DP-1");
        nanBright.sdrBrightnessNits = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!model.requestAssignment(nanBright));
    }

    // AGENT-GUARD: Every rejected mutation must leave the complete model
    // byte-identical: same revision, same snapshot bytes, atomic reject.
    QCOMPARE(model.revision(), 2u);
    const auto after = model.snapshot();
    QCOMPARE(after.revision, before.revision);
    QCOMPARE(after.lineageFingerprint, before.lineageFingerprint);
    QCOMPARE(after, before);
}

void ColorModelTest::testMaxOutputsAggregateCap()
{
    ColorModel model("epoch-1");
    for (quint32 i = 0; i < MaxOutputs; ++i) {
        const auto caps = createSampleCapabilities(QString("out-%1").arg(i), false, false);
        QVERIFY(model.updateCapabilities(caps));
    }
    QCOMPARE(model.snapshot().outputs.size(), static_cast<qsizetype>(MaxOutputs));

    // The 33rd distinct output is rejected...
    const auto extra = createSampleCapabilities("out-overflow", false, false);
    QVERIFY(!model.updateCapabilities(extra));
    QCOMPARE(model.snapshot().outputs.size(), static_cast<qsizetype>(MaxOutputs));

    // ...but refreshing an already-known output is still accepted
    const auto refresh = createSampleCapabilities("out-0", true, true);
    QVERIFY(model.updateCapabilities(refresh));

    // An assignment creating the 33rd output is also rejected
    OutputColorAssignment extraAssignment;
    extraAssignment.stableId = "out-overflow-2";
    QVERIFY(!model.requestAssignment(extraAssignment));
    QCOMPARE(model.snapshot().outputs.size(), static_cast<qsizetype>(MaxOutputs));
}

QTEST_MAIN(ColorModelTest)
#include "tst_color_model.moc"
