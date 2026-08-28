// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>
#include <functional>
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
    void testFingerprintFramingNoAmbiguity();
    void testFingerprintCoversEverySnapshotField();
    void testOutputLifecycleRemoval();
    void testEpochReset();
    void testSameEpochResetMonotonic();
    void testDefaultSrgbTruthfulSemantics();
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

void ColorModelTest::testFingerprintFramingNoAmbiguity()
{
    // AGENT-GUARD: the fingerprint encoding is domain-tagged and
    // length-delimited, so moving payload bytes between adjacent string
    // fields cannot collide the way an unframed concatenation would:
    // default="a"+profile="bc" and default="ab"+profile="c" both
    // concatenate to "abc" but must fingerprint differently.
    ColorCatalog catalogA;
    ColorCatalog catalogB;
    IccProfileDescriptor encodedA;
    IccProfileDescriptor encodedB;
    catalogA.defaultSrgbProfileId = "a";
    catalogB.defaultSrgbProfileId = "ab";
    encodedA.profileId = "bc";
    encodedB.profileId = "c";
    catalogA.profiles = {encodedA};
    catalogB.profiles = {encodedB};

    QCOMPARE(catalogA.defaultSrgbProfileId + encodedA.profileId,
             catalogB.defaultSrgbProfileId + encodedB.profileId);
    QVERIFY(computeLineageFingerprint("same", 0, catalogA, {}) !=
            computeLineageFingerprint("same", 0, catalogB, {}));
}

void ColorModelTest::testFingerprintCoversEverySnapshotField()
{
    // AGENT-GUARD: every semantically published snapshot field is framed
    // into the lineage fingerprint. Each block below mutates exactly one
    // field of an otherwise identical published state and requires a
    // changed fingerprint; a passing block that newly added fields forget
    // to frame would silently re-open the equal-fingerprint defect from
    // the Curie the 3rd review.
    ColorModel model("epoch-fp");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::System);
    QVERIFY(model.setCatalog({pSrgb}, "srgb-std"));
    const auto caps = createSampleCapabilities("DP-1", true, true);
    QVERIFY(model.updateCapabilities(caps));
    OutputColorAssignment req;
    req.stableId = "DP-1";
    req.profileId = "srgb-std";
    req.policy = OutputColorPolicy::SdrWcg;
    req.intent = RenderingIntent::RelativeColorimetric;
    req.sdrBrightnessGainApplied = true;
    QVERIFY(model.requestAssignment(req));

    const auto baseline = model.snapshot();
    const QString epoch = baseline.serviceEpoch;
    const quint64 revision = baseline.revision;
    const QByteArray baselineFp = baseline.lineageFingerprint;
    QVERIFY(!baselineFp.isEmpty());
    QVERIFY(baseline.outputs.size() == 1);
    QVERIFY(baseline.catalog.profiles.size() == 1);

    // Recomputation over identical inputs reproduces the published bytes.
    QCOMPARE(computeLineageFingerprint(epoch, revision, baseline.catalog, baseline.outputs), baselineFp);

    // --- epoch and revision ---
    QVERIFY(computeLineageFingerprint("other-epoch", revision, baseline.catalog, baseline.outputs) != baselineFp);
    QVERIFY(computeLineageFingerprint(epoch, revision + 1, baseline.catalog, baseline.outputs) != baselineFp);

    // --- catalog-level fields ---
    const auto catalogFingerprintOf = [&](const ColorCatalog &catalog) {
        return computeLineageFingerprint(epoch, revision, catalog, baseline.outputs);
    };
    {
        auto catalog = baseline.catalog;
        catalog.defaultSrgbProfileId = "other-default";
        QVERIFY(catalogFingerprintOf(catalog) != baselineFp);
    }
    {
        auto catalog = baseline.catalog;
        catalog.wireValid = !catalog.wireValid;
        QVERIFY(catalogFingerprintOf(catalog) != baselineFp);
    }

    // --- per-profile fields ---
    const auto mutateProfile = [&](const std::function<void(IccProfileDescriptor &)> &mutate) {
        auto catalog = baseline.catalog;
        mutate(catalog.profiles[0]);
        return computeLineageFingerprint(epoch, revision, catalog, baseline.outputs);
    };
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.profileId = "mutated-id"; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.displayName = "Mutated Name"; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.description = "Mutated description"; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.fileName = "mutated.icc"; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.origin = ProfileOrigin::EdidDerived; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.gamut = ColorSpaceGamut::AdobeRgb; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.transferFunction = TransferFunction::Hlg; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.rawHeader[40] = p.rawHeader[40] ^ char(0x01); }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.checksumSha256 = QByteArray(32, 'x'); }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.byteSize += 1; }) != baselineFp);
    QVERIFY(mutateProfile([](IccProfileDescriptor &p) { p.wireValid = !p.wireValid; }) != baselineFp);

    // --- per-output fields ---
    const auto mutateOutput = [&](const std::function<void(OutputColorState &)> &mutate) {
        auto outputs = baseline.outputs;
        mutate(outputs[0]);
        return computeLineageFingerprint(epoch, revision, baseline.catalog, outputs);
    };
    QVERIFY(mutateOutput([](OutputColorState &s) { s.stableId = "DP-2"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.activeProfileId = "other-active"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.degradedReason = DegradedReason::HdrUnsupported; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.isDegraded = !s.isDegraded; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.wireValid = !s.wireValid; }) != baselineFp);

    // capabilities fields
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.stableId = "DP-2"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportsWcg = !s.capabilities.supportsWcg; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportsHdr = !s.capabilities.supportsHdr; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportedGamuts.append(ColorSpaceGamut::AdobeRgb); }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportedGamuts.clear(); }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportedGamuts.removeFirst(); }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportedTransferFunctions.append(TransferFunction::Gamma22); }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.supportedTransferFunctions.clear(); }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.minLuminanceNits += 0.1; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.maxLuminanceNits += 0.1; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.maxFullFrameLuminanceNits += 0.1; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.autoColorManagementAvailable = !s.capabilities.autoColorManagementAvailable; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.capabilities.wireValid = !s.capabilities.wireValid; }) != baselineFp);

    // requested assignment fields
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.stableId = "DP-2"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.profileId = "other-requested"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.policy = OutputColorPolicy::HdrEnabled; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.intent = RenderingIntent::Saturation; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.sdrBrightnessGainApplied = !s.requestedAssignment.sdrBrightnessGainApplied; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.sdrBrightnessNits += 1.0; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.requestedAssignment.wireValid = !s.requestedAssignment.wireValid; }) != baselineFp);

    // applied assignment fields
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.stableId = "DP-2"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.profileId = "other-applied"; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.policy = OutputColorPolicy::AutoColorManagement; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.intent = RenderingIntent::AbsoluteColorimetric; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.sdrBrightnessGainApplied = !s.appliedAssignment.sdrBrightnessGainApplied; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.sdrBrightnessNits += 1.0; }) != baselineFp);
    QVERIFY(mutateOutput([](OutputColorState &s) { s.appliedAssignment.wireValid = !s.appliedAssignment.wireValid; }) != baselineFp);
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

void ColorModelTest::testSameEpochResetMonotonic()
{
    // AGENT-GUARD: resetting to the epoch already in force must never
    // regress the model-monotonic revision inside that epoch (the exact
    // 1 -> 0 regression the Curie the 3rd harness reproduced).
    ColorModel model("epoch-stable");
    const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB");
    model.setCatalog({pSrgb}, "srgb-std");
    QCOMPARE(model.revision(), 1u);

    model.resetEpoch("epoch-stable");
    QCOMPARE(model.serviceEpoch(), QString("epoch-stable"));
    QCOMPARE(model.revision(), 1u);
    QVERIFY(model.validateLineage("epoch-stable", 1u));

    // Further mutations still advance monotonically from the preserved
    // revision, so no revision value inside this epoch is ever reused.
    const auto caps = createSampleCapabilities("DP-1", false, false);
    QVERIFY(model.updateCapabilities(caps));
    QCOMPARE(model.revision(), 2u);

    // A genuinely distinct epoch restarts lineage at revision zero.
    model.resetEpoch("epoch-next");
    QCOMPARE(model.serviceEpoch(), QString("epoch-next"));
    QCOMPARE(model.revision(), 0u);
}

void ColorModelTest::testDefaultSrgbTruthfulSemantics()
{
    // AGENT-GUARD: a catalog whose only profile is BT.2020 has no truthful
    // sRGB fallback: the default fails closed to empty instead of adopting
    // a non-sRGB profile, and a capability-clamped SDR policy publishes no
    // applied profile rather than BT.2020 applied truth.
    {
        ColorModel model("epoch-1");
        const auto pBt2020 = createSampleProfile("bt2020", "BT.2020", ProfileOrigin::BuiltIn,
                                                 ColorSpaceGamut::Bt2020);
        QVERIFY(model.setCatalog({pBt2020}, "bt2020"));
        QVERIFY(model.snapshot().catalog.defaultSrgbProfileId.isEmpty());

        const auto caps = createSampleCapabilities("DP-1", false, false);
        QVERIFY(model.updateCapabilities(caps));
        OutputColorAssignment request;
        request.stableId = "DP-1";
        request.profileId = "bt2020";
        request.policy = OutputColorPolicy::HdrEnabled;
        QVERIFY(model.requestAssignment(request));

        const auto state = model.outputState("DP-1");
        QVERIFY(state.has_value());
        QVERIFY(state->isDegraded);
        QCOMPARE(state->degradedReason, DegradedReason::HdrUnsupported);
        QCOMPARE(state->appliedAssignment.policy, OutputColorPolicy::SdrSrgb);
        QVERIFY(state->appliedAssignment.profileId.isEmpty());
        QVERIFY(state->activeProfileId.isEmpty());
        // Requested intent stays truthful for observers.
        QCOMPARE(state->requestedAssignment.profileId, QString("bt2020"));
        QCOMPARE(state->requestedAssignment.policy, OutputColorPolicy::HdrEnabled);
    }

    // The caller's chosen default is honored only with sRGB gamut/transfer
    // semantics; a non-sRGB choice deterministically falls to the first
    // sorted sRGB entry instead.
    {
        ColorModel model("epoch-2");
        const auto pBt2020 = createSampleProfile("a-bt2020", "A BT.2020", ProfileOrigin::BuiltIn,
                                                 ColorSpaceGamut::Bt2020);
        const auto pSrgbZ = createSampleProfile("z-srgb", "Z sRGB", ProfileOrigin::BuiltIn);
        const auto pSrgbA = createSampleProfile("a-srgb", "A sRGB", ProfileOrigin::BuiltIn);

        QVERIFY(model.setCatalog({pBt2020, pSrgbZ, pSrgbA}, "a-bt2020"));
        QCOMPARE(model.snapshot().catalog.defaultSrgbProfileId, QString("a-srgb"));

        QVERIFY(model.setCatalog({pBt2020, pSrgbZ, pSrgbA}, "z-srgb"));
        QCOMPARE(model.snapshot().catalog.defaultSrgbProfileId, QString("z-srgb"));

        QVERIFY(model.setCatalog({pBt2020, pSrgbZ, pSrgbA}));
        QCOMPARE(model.snapshot().catalog.defaultSrgbProfileId, QString("a-srgb"));
    }

    // Registering an sRGB profile into a default-less catalog installs the
    // deterministic sRGB default; removing it fails closed again.
    {
        ColorModel model("epoch-3");
        const auto pBt2020 = createSampleProfile("bt2020", "BT.2020", ProfileOrigin::BuiltIn,
                                                 ColorSpaceGamut::Bt2020);
        QVERIFY(model.registerProfile(pBt2020));
        QVERIFY(model.snapshot().catalog.defaultSrgbProfileId.isEmpty());

        const auto pSrgb = createSampleProfile("srgb-std", "Standard sRGB", ProfileOrigin::System);
        QVERIFY(model.registerProfile(pSrgb));
        QCOMPARE(model.snapshot().catalog.defaultSrgbProfileId, QString("srgb-std"));

        QVERIFY(model.removeProfile("srgb-std"));
        QVERIFY(model.snapshot().catalog.defaultSrgbProfileId.isEmpty());
    }
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
