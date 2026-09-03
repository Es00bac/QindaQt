// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/clipboard_client/clipboard_client.h"
#include "qindaqt/services/clipboard_client/qt_clipboard_transport.h"
#include "qindaqt/services/clipboard_model/clipboard_descriptor.h"
#include "qindaqt/services/clipboard_protocol/clipboard_validation.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"

#include <QMetaObject>
#include <QMetaType>

#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::Shell {
namespace {

using ClipboardCapabilityGrants = std::pair<bool, bool>;
using ClipboardEntryId = Services::ClipboardModel::EntryId;

constexpr auto kConsentKey = "services.clipboardHistory";

ClipboardCapabilityGrants clipboardGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("clipboard"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }

    bool read = false;
    bool write = false;
    for (const auto &decision : evaluated.decisions) {
        if (decision.capability == Applets::Capability::ClipboardRead) {
            read = decision.granted();
        } else if (decision.capability == Applets::Capability::ClipboardWrite) {
            write = decision.granted();
        }
    }
    return {read, write};
}

bool explicitHistoryConsent(
    const Services::SettingsClient::SettingsClient &settings)
{
    if (settings.state() != Services::SettingsClient::ClientState::Ready
        || !settings.snapshot().has_value()) {
        return false;
    }
    const auto &snapshot = *settings.snapshot();
    const QVariant value = snapshot.values.value(QLatin1String(kConsentKey));
    const QVariant source = snapshot.sourceLayers.value(QLatin1String(kConsentKey));
    return value.metaType().id() == QMetaType::Bool && value.toBool()
        && source.metaType().id() == QMetaType::QString
        && source.toString() == QLatin1String("user-overrides");
}

class ClipboardClientBridge final
    : public ShellClipboardApplet::ClipboardClientInterface
{
public:
    ClipboardClientBridge(Services::Clipboard::ClipboardClient &client,
                          Services::SettingsClient::SettingsClient &settings,
                          Services::Clipboard::ClipboardTransport &transport)
        : m_client(client)
        , m_settings(settings)
        , m_transport(transport)
    {
        connect(&m_client, &Services::Clipboard::ClipboardClient::stateChanged,
                this, [this] { refresh(); });
        connect(&m_client, &Services::Clipboard::ClipboardClient::snapshotChanged,
                this, [this] { refresh(); });
        connect(&m_client, &Services::Clipboard::ClipboardClient::operationCompleted,
                this, [this](quint64 requestId,
                             const Services::Clipboard::OperationResult &result) {
                    finishOperation(requestId, result);
                });
        connect(&m_settings,
                &Services::SettingsClient::SettingsClient::stateChanged,
                this, [this] { refresh(); });
        connect(&m_transport, &Services::Clipboard::ClipboardTransport::ownerChanged,
                this, [this](const QString &owner) {
                    if (owner == m_transportOwner) {
                        return;
                    }
                    m_transportOwner = owner;
                    m_wireSnapshot.reset();
                    publishUnavailable(owner.isEmpty()
                                           ? QStringLiteral("service-unavailable")
                                           : QStringLiteral("fetching-snapshot"));
                });
        connect(&m_transport, &Services::Clipboard::ClipboardTransport::snapshotReply,
                this, [this](const QString &owner, quint64, bool success,
                             const Services::Clipboard::Snapshot &snapshot,
                             const QString &reason) {
                    if (owner != m_transportOwner || owner.isEmpty()) {
                        return;
                    }
                    if (!success
                        || !Services::Clipboard::validateSnapshot(snapshot).accepted) {
                        m_wireSnapshot.reset();
                        publishUnavailable(reason.isEmpty()
                                               ? QStringLiteral("malformed-snapshot")
                                               : reason);
                        return;
                    }
                    m_wireSnapshot = snapshot;
                    publishSnapshot(snapshot, owner);
                });
        connect(&m_settings,
                &Services::SettingsClient::SettingsClient::snapshotChanged,
                this, [this] { refresh(); });
        refresh();
    }

    [[nodiscard]] ShellClipboardApplet::ClientState clientState() const noexcept override
    {
        return m_state;
    }
    [[nodiscard]] QString reasonCode() const override { return m_reason; }
    [[nodiscard]] QString owner() const override { return m_owner; }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override
    {
        return !m_owner.isEmpty();
    }
    [[nodiscard]] bool isLocked() const noexcept override { return false; }
    [[nodiscard]] Services::ClipboardModel::HistorySnapshot snapshot() const override
    {
        return m_snapshot;
    }

    quint64 requestPromote(ClipboardEntryId id, quint32 expectedGeneration,
                           quint64) override
    {
        return dispatchEntry(id, expectedGeneration,
                             [this, id] { return m_client.copy(id); });
    }

    quint64 requestRemove(ClipboardEntryId id,
                          quint32 expectedGeneration) override
    {
        return dispatchEntry(id, expectedGeneration,
                             [this, id] { return m_client.remove(id); });
    }

    quint64 requestSetPinned(ClipboardEntryId id, bool,
                             quint32 expectedGeneration) override
    {
        const quint64 requestId = nextRequestId();
        ShellClipboardApplet::OperationOutcome outcome;
        outcome.id = id;
        if (!canMutate(expectedGeneration)) {
            outcome = refusedOutcome(id);
        } else {
            outcome.code = ShellClipboardApplet::OperationErrorCode::Failed;
            outcome.message = QStringLiteral(
                "Pinning is unavailable through Clipboard1 version 1.");
        }
        Q_EMIT operationCompleted(requestId, outcome);
        return requestId;
    }

    quint64 requestClear(Services::ClipboardModel::ClearScope scope,
                         quint32 expectedGeneration) override
    {
        const quint64 requestId = nextRequestId();
        if (!canMutate(expectedGeneration)) {
            Q_EMIT operationCompleted(requestId, refusedOutcome({}));
            return requestId;
        }
        const quint64 clientId =
            m_client.clear(scope == Services::ClipboardModel::ClearScope::All);
        m_pending.insert(clientId, Pending{requestId, {}});
        return requestId;
    }

    quint64 requestSearch(const QString &query, quint32 expectedGeneration,
                          int maxResults) override
    {
        const quint64 requestId = nextRequestId();
        Services::ClipboardModel::SearchOutcome outcome;
        if (!canObserve(expectedGeneration)) {
            outcome.error = Services::ClipboardModel::ClipboardError::StaleGeneration;
        } else {
            const int limit = qBound(1, maxResults,
                                     Services::ClipboardModel::kMaxEntries);
            for (const auto &entry : m_snapshot.entries) {
                if (!entry.preview.contains(query, Qt::CaseInsensitive)
                    && !entry.sourceLabel.contains(query, Qt::CaseInsensitive)) {
                    continue;
                }
                if (outcome.matches.size() == limit) {
                    outcome.truncated = true;
                    break;
                }
                outcome.matches.append(entry);
            }
        }
        Q_EMIT searchCompleted(requestId, outcome);
        return requestId;
    }

private:
    struct Pending {
        quint64 bridgeId = 0;
        ClipboardEntryId entry;
    };

    [[nodiscard]] quint64 nextRequestId()
    {
        if (m_nextRequestId == std::numeric_limits<quint64>::max()) {
            publishUnavailable(QStringLiteral("request-id-exhausted"));
        }
        return m_nextRequestId++;
    }

    [[nodiscard]] bool canObserve(quint32 generation) const noexcept
    {
        return m_state == ShellClipboardApplet::ClientState::Ready
            && generation == m_snapshot.generation;
    }

    [[nodiscard]] bool canMutate(quint32 generation) const noexcept
    {
        return canObserve(generation) && m_snapshot.historyEnabled
            && m_snapshot.privacyAllowed;
    }

    [[nodiscard]] ShellClipboardApplet::OperationOutcome refusedOutcome(
        ClipboardEntryId id) const
    {
        ShellClipboardApplet::OperationOutcome outcome;
        outcome.id = id;
        if (!isOwnerAvailable()) {
            outcome.code = ShellClipboardApplet::OperationErrorCode::OwnerLost;
            outcome.message = QStringLiteral("Clipboard service is unavailable.");
        } else if (!m_snapshot.historyEnabled) {
            outcome.code = ShellClipboardApplet::OperationErrorCode::HistoryDisabled;
            outcome.message = QStringLiteral("Clipboard history is disabled.");
        } else if (!m_snapshot.privacyAllowed) {
            outcome.code = ShellClipboardApplet::OperationErrorCode::PrivacyDenied;
            outcome.message = QStringLiteral(
                "Clipboard access is restricted by privacy policy.");
        } else {
            outcome.code = ShellClipboardApplet::OperationErrorCode::StaleGeneration;
            outcome.message = QStringLiteral("Clipboard history has changed.");
        }
        return outcome;
    }

    template<typename Dispatch>
    quint64 dispatchEntry(ClipboardEntryId id, quint32 expectedGeneration,
                          Dispatch dispatch)
    {
        const quint64 requestId = nextRequestId();
        if (!canMutate(expectedGeneration) || id.generation != expectedGeneration) {
            Q_EMIT operationCompleted(requestId, refusedOutcome(id));
            return requestId;
        }
        const quint64 clientId = dispatch();
        m_pending.insert(clientId, Pending{requestId, id});
        return requestId;
    }

    void refresh()
    {
        if (m_wireSnapshot.has_value() && !m_transportOwner.isEmpty()) {
            publishSnapshot(*m_wireSnapshot, m_transportOwner);
            return;
        }
        if (m_client.state() != Services::Clipboard::ClientState::Ready
            || !m_client.hasSnapshot()) {
            // ClipboardHistoryModel's terminal authority purge changes flags
            // and clears content at an equal lineage. ClipboardClient rejects
            // that non-duplicate as a generic contradiction; the validated
            // transport reply connected above immediately supplies the
            // narrower applet fence. Do not create an observable owner-loss
            // edge between those two same-signal callbacks.
            if (m_client.reasonCode() == QLatin1String("malformed-snapshot")
                && !m_transportOwner.isEmpty()) {
                return;
            }
            publishUnavailable(m_client.reasonCode().isEmpty()
                                   ? QStringLiteral("clipboard-unavailable")
                                   : m_client.reasonCode());
            return;
        }
        publishSnapshot(m_client.snapshot(), m_client.owner());
    }

    void publishSnapshot(const Services::Clipboard::Snapshot &source,
                         const QString &serviceOwner)
    {
        if (m_settings.state() != Services::SettingsClient::ClientState::Ready
            || !m_settings.snapshot().has_value()) {
            publishUnavailable(QStringLiteral("clipboard-consent-unavailable"));
            return;
        }

        const auto decoded = Services::ClipboardModel::decodeDescriptorList(
            source.descriptorList);
        if (!decoded.accepted()) {
            publishUnavailable(QStringLiteral("clipboard-descriptor-malformed"));
            return;
        }
        const bool consent = explicitHistoryConsent(m_settings);
        if (!consent && (source.historyEnabled || !decoded.descriptors.isEmpty())) {
            // AGENT-GUARD: never forge authority withdrawal at the old model
            // generation. Withhold immediately, then wait for Clipboard1's
            // purged, generation-advanced snapshot to expose Disabled.
            publishUnavailable(QStringLiteral("clipboard-consent-denied"));
            return;
        }

        Services::ClipboardModel::HistorySnapshot projected;
        projected.generation = source.generation;
        projected.revision = source.revision;
        projected.historyEnabled = source.historyEnabled && consent;
        projected.privacyAllowed = source.privacyAllowed;
        projected.entries = decoded.descriptors;
        for (const auto &entry : projected.entries) {
            for (const auto &format : entry.formats) {
                projected.totalPayloadBytes += format.payloadBytes;
            }
        }
        const QString settingsOwner = m_settings.snapshot()->owner;
        if (serviceOwner.isEmpty() || settingsOwner.isEmpty()) {
            publishUnavailable(QStringLiteral("owner-unavailable"));
            return;
        }
        const QString projectedOwner = serviceOwner + QLatin1Char('|') + settingsOwner;
        const bool ownerChanged = projectedOwner != m_owner;
        const bool snapshotUpdated = projected != m_snapshot;
        m_owner = projectedOwner;
        m_snapshot = std::move(projected);
        m_state = ShellClipboardApplet::ClientState::Ready;
        m_reason = QStringLiteral("ready");
        if (ownerChanged) {
            Q_EMIT stateChanged(m_state, m_reason);
        }
        if (snapshotUpdated || ownerChanged) {
            Q_EMIT snapshotChanged(m_snapshot);
        }
    }

    void publishUnavailable(const QString &reason)
    {
        const bool changed = m_state != ShellClipboardApplet::ClientState::Unavailable
            || m_reason != reason || !m_owner.isEmpty();
        m_state = ShellClipboardApplet::ClientState::Unavailable;
        m_reason = reason;
        m_owner.clear();
        m_snapshot = {};
        if (changed) {
            Q_EMIT stateChanged(m_state, m_reason);
        }
    }

    static ShellClipboardApplet::OperationErrorCode mapStatus(
        const Services::Clipboard::OperationResult &result)
    {
        using AppletError = ShellClipboardApplet::OperationErrorCode;
        using Status = Services::Clipboard::OperationStatus;
        if (result.status == Status::Succeeded) {
            return AppletError::None;
        }
        if (result.status == Status::Busy) {
            return AppletError::Busy;
        }
        if (result.reasonCode == QLatin1String("history-disabled")) {
            return AppletError::HistoryDisabled;
        }
        if (result.reasonCode == QLatin1String("privacy-denied")) {
            return AppletError::PrivacyDenied;
        }
        if (result.reasonCode == QLatin1String("stale-generation")
            || result.reasonCode == QLatin1String("stale-revision")) {
            return AppletError::StaleGeneration;
        }
        if (result.reasonCode == QLatin1String("unknown-entry")) {
            return AppletError::UnknownEntry;
        }
        if (result.reasonCode == QLatin1String("owner-replaced")
            || result.reasonCode == QLatin1String("unavailable")) {
            return AppletError::OwnerLost;
        }
        return AppletError::Failed;
    }

    void finishOperation(quint64 clientId,
                         const Services::Clipboard::OperationResult &result)
    {
        const auto it = m_pending.constFind(clientId);
        if (it == m_pending.constEnd()) {
            return;
        }
        const Pending pending = it.value();
        m_pending.erase(it);
        ShellClipboardApplet::OperationOutcome outcome;
        outcome.id = pending.entry;
        outcome.code = mapStatus(result);
        if (!outcome.ok()) {
            outcome.message = result.reasonCode.isEmpty()
                ? QStringLiteral("Clipboard operation failed.")
                : QStringLiteral("Clipboard operation failed: %1.")
                      .arg(result.reasonCode);
        }
        Q_EMIT operationCompleted(pending.bridgeId, outcome);
    }

    Services::Clipboard::ClipboardClient &m_client;
    Services::SettingsClient::SettingsClient &m_settings;
    Services::Clipboard::ClipboardTransport &m_transport;
    ShellClipboardApplet::ClientState m_state =
        ShellClipboardApplet::ClientState::Unavailable;
    QString m_reason = QStringLiteral("clipboard-unavailable");
    QString m_owner;
    Services::ClipboardModel::HistorySnapshot m_snapshot;
    QString m_transportOwner;
    std::optional<Services::Clipboard::Snapshot> m_wireSnapshot;
    QHash<quint64, Pending> m_pending;
    quint64 m_nextRequestId = 1;
};

} // namespace

ClipboardAppletComposition::ClipboardAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    const QDBusConnection &sessionBus)
    : m_ownedTransport(
          std::make_unique<Services::Clipboard::QtClipboardTransport>(sessionBus))
{
    compose(catalog, policy, settingsClient, *m_ownedTransport);
}

ClipboardAppletComposition::ClipboardAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Clipboard::ClipboardTransport &transport)
{
    compose(catalog, policy, settingsClient, transport);
}

ClipboardAppletComposition::~ClipboardAppletComposition()
{
    if (m_client) {
        m_client->stop();
    }
}

void ClipboardAppletComposition::compose(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Clipboard::ClipboardTransport &transport)
{
    const auto [read, write] = clipboardGrants(catalog, policy);
    m_client = std::make_unique<Services::Clipboard::ClipboardClient>(&transport);
    m_bridge = std::make_unique<ClipboardClientBridge>(
        *m_client, settingsClient, transport);
    m_access = std::make_unique<ShellClipboardApplet::ClipboardAppletController>(
        m_bridge.get(), read, write);
    if (read) {
        m_client->start();
    }
}

ShellClipboardApplet::ClipboardAppletController *
ClipboardAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
