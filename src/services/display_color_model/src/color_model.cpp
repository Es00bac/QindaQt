// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/display_color_model/color_model.h"
#include "qindaqt/services/display_color_model/color_limits.h"
#include "qindaqt/services/display_color_model/color_validation.h"

#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <algorithm>

namespace QindaQt::DisplayColor
{

namespace
{

// AGENT-GUARD: a truthful sRGB fallback requires sRGB gamut AND sRGB
// transfer semantics. Accepting any existing profile as the "default sRGB"
// would publish incoherent applied color truth — for example a BT.2020
// profile under a capability-clamped SDR sRGB policy.
bool hasSrgbSemantics(const IccProfileDescriptor &profile)
{
    return profile.gamut == ColorSpaceGamut::Srgb &&
           profile.transferFunction == TransferFunction::Srgb;
}

} // namespace

ColorModel::ColorModel(const QString &serviceEpoch)
    : m_serviceEpoch(serviceEpoch.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : serviceEpoch)
{
}

QString ColorModel::serviceEpoch() const
{
    return m_serviceEpoch;
}

quint64 ColorModel::revision() const
{
    return m_revision;
}

void ColorModel::resetEpoch(const QString &newEpoch)
{
    // AGENT-GUARD: resetting to the epoch already in force must never
    // regress the model-monotonic revision (1 -> 0) inside one epoch; the
    // request is a no-op. A distinct (or generated) epoch starts fresh
    // lineage at revision zero.
    if (!newEpoch.isEmpty() && newEpoch == m_serviceEpoch) {
        return;
    }
    m_serviceEpoch = newEpoch.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : newEpoch;
    m_revision = 0;
    reevaluateAllOutputs();
}

void ColorModel::advanceRevision()
{
    ++m_revision;
}

int ColorModel::knownOutputCount() const
{
    QSet<QString> outputs;
    for (auto it = m_capabilitiesByOutput.keyBegin(); it != m_capabilitiesByOutput.keyEnd(); ++it) {
        outputs.insert(*it);
    }
    for (auto it = m_requestedByOutput.keyBegin(); it != m_requestedByOutput.keyEnd(); ++it) {
        outputs.insert(*it);
    }
    return static_cast<int>(outputs.size());
}

bool ColorModel::setCatalog(const QList<IccProfileDescriptor> &profiles, const QString &defaultSrgbId)
{
    const auto normalized = normalizeAndSortCatalog(profiles);

    m_profilesById.clear();
    for (const auto &prof : normalized) {
        m_profilesById.insert(prof.profileId, prof);
    }

    m_catalog.profiles = normalized;
    // AGENT-GUARD: the caller's default is honored only when it exists and
    // carries truthful sRGB gamut/transfer semantics; anything else falls
    // through to the deterministic first sorted sRGB entry or fails closed
    // to an empty default.
    if (!defaultSrgbId.isEmpty() && m_profilesById.contains(defaultSrgbId) &&
        hasSrgbSemantics(m_profilesById.value(defaultSrgbId))) {
        m_catalog.defaultSrgbProfileId = defaultSrgbId;
    } else {
        m_catalog.defaultSrgbProfileId.clear();
        refreshDefaultSrgbProfile();
    }

    advanceRevision();
    reevaluateAllOutputs();
    return true;
}

bool ColorModel::registerProfile(const IccProfileDescriptor &profile)
{
    if (validateProfileDescriptor(profile) != ProfileValidationStatus::Valid) {
        return false;
    }

    if (m_profilesById.size() >= static_cast<int>(MaxProfilesInCatalog) &&
        !m_profilesById.contains(profile.profileId)) {
        return false;
    }

    m_profilesById.insert(profile.profileId, profile);

    QList<IccProfileDescriptor> profileList = m_profilesById.values();
    m_catalog.profiles = normalizeAndSortCatalog(profileList);
    refreshDefaultSrgbProfile();

    advanceRevision();
    reevaluateAllOutputs();
    return true;
}

bool ColorModel::removeProfile(const QString &profileId)
{
    if (!m_profilesById.contains(profileId)) {
        return false;
    }

    m_profilesById.remove(profileId);
    QList<IccProfileDescriptor> profileList = m_profilesById.values();
    m_catalog.profiles = normalizeAndSortCatalog(profileList);
    refreshDefaultSrgbProfile();

    advanceRevision();
    reevaluateAllOutputs();
    return true;
}

std::optional<IccProfileDescriptor> ColorModel::findProfile(const QString &profileId) const
{
    auto it = m_profilesById.find(profileId);
    if (it != m_profilesById.end()) {
        return *it;
    }
    return std::nullopt;
}

bool ColorModel::updateCapabilities(const OutputColorCapabilities &capabilities)
{
    if (!validateOutputCapabilities(capabilities)) {
        return false;
    }

    // AGENT-GUARD: The 32-output aggregate cap bounds model memory against a
    // hostile inventory flood; updating an already-known output never exceeds it.
    if (!m_capabilitiesByOutput.contains(capabilities.stableId) &&
        !m_requestedByOutput.contains(capabilities.stableId) &&
        knownOutputCount() >= static_cast<int>(MaxOutputs)) {
        return false;
    }

    m_capabilitiesByOutput.insert(capabilities.stableId, capabilities);
    advanceRevision();
    reevaluateOutput(capabilities.stableId);
    return true;
}

bool ColorModel::removeOutput(const QString &stableId)
{
    if (!m_capabilitiesByOutput.contains(stableId) && !m_requestedByOutput.contains(stableId)) {
        return false;
    }

    m_capabilitiesByOutput.remove(stableId);
    m_requestedByOutput.remove(stableId);
    m_lkgByOutput.remove(stableId);
    m_stateByOutput.remove(stableId);

    advanceRevision();
    return true;
}

std::optional<OutputColorCapabilities> ColorModel::capabilities(const QString &stableId) const
{
    auto it = m_capabilitiesByOutput.find(stableId);
    if (it != m_capabilitiesByOutput.end()) {
        return *it;
    }
    return std::nullopt;
}

bool ColorModel::requestAssignment(const OutputColorAssignment &assignment)
{
    if (!validateOutputAssignment(assignment)) {
        return false;
    }

    // AGENT-GUARD: Assignments for previously unknown outputs also create
    // state, so they fall under the same 32-output aggregate cap.
    if (!m_capabilitiesByOutput.contains(assignment.stableId) &&
        !m_requestedByOutput.contains(assignment.stableId) &&
        knownOutputCount() >= static_cast<int>(MaxOutputs)) {
        return false;
    }

    m_requestedByOutput.insert(assignment.stableId, assignment);
    advanceRevision();
    reevaluateOutput(assignment.stableId);
    return true;
}

std::optional<OutputColorState> ColorModel::outputState(const QString &stableId) const
{
    auto it = m_stateByOutput.find(stableId);
    if (it != m_stateByOutput.end()) {
        return *it;
    }
    return std::nullopt;
}

bool ColorModel::validateLineage(const QString &epoch, quint64 revision) const
{
    // AGENT-CONTRACT: Reject out-of-order or mismatched epoch publications
    // to preserve causal consistency with Display1 services and clients.
    // Acceptance requires exact equality: a stale (older) revision is not
    // current truth, and a newer revision can only be published by the model
    // itself, so a foreign claim to one is rejected fail-closed.
    if (epoch != m_serviceEpoch) {
        return false;
    }
    return revision == m_revision;
}

void ColorModel::refreshDefaultSrgbProfile()
{
    // Keep an existing default only while it still resolves and still has
    // truthful sRGB semantics; otherwise deterministically adopt the first
    // sorted sRGB entry. m_catalog.profiles is sorted at this point, so the
    // choice is independent of registration order. When no sRGB-semantic
    // profile exists the default stays empty and SDR fallbacks fail closed
    // with no applied profile rather than publishing a non-sRGB default.
    if (!m_catalog.defaultSrgbProfileId.isEmpty()) {
        const auto it = m_profilesById.find(m_catalog.defaultSrgbProfileId);
        if (it != m_profilesById.end() && hasSrgbSemantics(*it)) {
            return;
        }
        m_catalog.defaultSrgbProfileId.clear();
    }
    for (const auto &profile : m_catalog.profiles) {
        if (hasSrgbSemantics(profile)) {
            m_catalog.defaultSrgbProfileId = profile.profileId;
            return;
        }
    }
}

void ColorModel::reevaluateOutput(const QString &stableId)
{
    OutputColorState state;
    state.stableId = stableId;

    const auto capIt = m_capabilitiesByOutput.find(stableId);
    if (capIt != m_capabilitiesByOutput.end()) {
        state.capabilities = *capIt;
    } else {
        state.capabilities.stableId = stableId;
    }

    OutputColorAssignment req;
    const auto reqIt = m_requestedByOutput.find(stableId);
    if (reqIt != m_requestedByOutput.end()) {
        req = *reqIt;
    } else {
        req.stableId = stableId;
        req.policy = OutputColorPolicy::SdrSrgb;
        req.profileId = m_catalog.defaultSrgbProfileId;
    }
    state.requestedAssignment = req;

    OutputColorAssignment applied = req;
    DegradedReason degraded = DegradedReason::None;
    bool policyClamped = false;

    // Check HDR capability requirement
    if (req.policy == OutputColorPolicy::HdrEnabled && !state.capabilities.supportsHdr) {
        degraded = DegradedReason::HdrUnsupported;
        applied.policy = OutputColorPolicy::SdrSrgb;
        policyClamped = true;
    }

    // Check WCG capability requirement
    if (req.policy == OutputColorPolicy::SdrWcg && !state.capabilities.supportsWcg) {
        degraded = DegradedReason::WcgUnsupported;
        applied.policy = OutputColorPolicy::SdrSrgb;
        policyClamped = true;
    }

    // Resolve profile
    QString targetProfileId = req.profileId;
    if (targetProfileId.isEmpty()) {
        targetProfileId = m_catalog.defaultSrgbProfileId;
    }

    if (policyClamped) {
        // AGENT-GUARD: A capability-clamped SDR policy must not keep the
        // requested HDR/WCG profile; applied truth falls back to the default
        // sRGB profile so the published state stays coherent and fail-closed.
        applied.profileId = m_catalog.defaultSrgbProfileId;
    } else if (!targetProfileId.isEmpty()) {
        auto profIt = m_profilesById.find(targetProfileId);
        if (profIt == m_profilesById.end()) {
            // AGENT-GUARD: If requested profile is not found, report ProfileNotFound,
            // fall back to LKG if valid or default sRGB to avoid unbounded display corruption.
            if (degraded == DegradedReason::None) {
                degraded = DegradedReason::ProfileNotFound;
            }
            const auto lkgIt = m_lkgByOutput.find(stableId);
            if (lkgIt != m_lkgByOutput.end() && m_profilesById.contains(lkgIt->profileId)) {
                applied.profileId = lkgIt->profileId;
            } else {
                applied.profileId = m_catalog.defaultSrgbProfileId;
            }
        } else {
            const auto status = validateProfileDescriptor(*profIt);
            if (status != ProfileValidationStatus::Valid) {
                if (degraded == DegradedReason::None) {
                    degraded = DegradedReason::ProfileInvalid;
                }
                applied.profileId = m_catalog.defaultSrgbProfileId;
            } else {
                applied.profileId = targetProfileId;
            }
        }
    } else {
        applied.profileId.clear();
    }

    state.appliedAssignment = applied;
    state.activeProfileId = applied.profileId;
    state.degradedReason = degraded;
    state.isDegraded = (degraded != DegradedReason::None);

    // If completely valid and non-degraded, record as LKG
    if (!state.isDegraded && !applied.profileId.isEmpty()) {
        m_lkgByOutput.insert(stableId, applied);
    }

    m_stateByOutput.insert(stableId, state);
}

void ColorModel::reevaluateAllOutputs()
{
    QSet<QString> allOutputs;
    for (auto it = m_capabilitiesByOutput.keyBegin(); it != m_capabilitiesByOutput.keyEnd(); ++it) {
        allOutputs.insert(*it);
    }
    for (auto it = m_requestedByOutput.keyBegin(); it != m_requestedByOutput.keyEnd(); ++it) {
        allOutputs.insert(*it);
    }

    for (const auto &id : allOutputs) {
        reevaluateOutput(id);
    }
}

ColorModelSnapshot ColorModel::snapshot() const
{
    ColorModelSnapshot snap;
    snap.schemaVersion = 1;
    snap.serviceEpoch = m_serviceEpoch;
    snap.revision = m_revision;
    snap.catalog = m_catalog;

    QList<OutputColorState> states;
    states.reserve(m_stateByOutput.size());
    for (const auto &st : m_stateByOutput) {
        states.append(st);
    }

    std::sort(states.begin(), states.end(), [](const OutputColorState &a, const OutputColorState &b) {
        return a.stableId < b.stableId;
    });

    snap.outputs = states;
    snap.lineageFingerprint = computeLineageFingerprint(snap.serviceEpoch, snap.revision, snap.catalog, snap.outputs);
    snap.wireValid = true;

    return snap;
}

} // namespace QindaQt::DisplayColor
