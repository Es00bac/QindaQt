// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_presenter.h"
#include "folder_navigations.h"
#include "../model/navigation_controller.h"
#include <QDir>
#include <QVariantMap>
#include <algorithm>

namespace QindaQt::Apps::FileManager {
namespace Media = QindaQt::RemovableMedia;
namespace {
bool sameOwner(const Media::Lineage &a, const Media::Lineage &b) {
    return a.owner == b.owner && a.epoch == b.epoch;
}
bool within(const QString &path, const QString &root) {
    const auto clean = QDir::cleanPath(path), base = QDir::cleanPath(root);
    return clean == base || clean.startsWith(base == QStringLiteral("/") ? base : base + '/');
}
struct RootMatch {
    const Media::VolumeRow *row = nullptr;
    QString root;
    bool ambiguous = false;
};
RootMatch locationAt(const Media::Snapshot &snapshot, const QString &path) {
    RootMatch best;
    for (const auto &row : snapshot.rows) {
        if (row.mountState != Media::MountState::Mounted) continue;
        for (const auto &root : row.mountRoots) {
            if (!within(path, root)) continue;
            if (root.size() > best.root.size()) best = {&row, root, false};
            else if (root.size() == best.root.size() && best.row
                && row.attachment != best.row->attachment) best.ambiguous = true;
        }
    }
    return best;
}
const Media::VolumeRow *rowFor(const Media::Snapshot &snapshot, const Media::Attachment &attachment) {
    const auto row = std::find_if(snapshot.rows.cbegin(), snapshot.rows.cend(),
        [&](const auto &value) { return value.attachment == attachment; });
    return row == snapshot.rows.cend() ? nullptr : &*row;
}
QString reason(Media::DisabledReason value) {
    switch (value) {
    case Media::DisabledReason::None: return {};
    case Media::DisabledReason::Locked: return QObject::tr("Unlock this device in Removable Media.");
    case Media::DisabledReason::Busy: return QObject::tr("A device operation is still running.");
    case Media::DisabledReason::ReadOnly: return QObject::tr("This device is read-only.");
    case Media::DisabledReason::NotMounted: return QObject::tr("This device is not mounted.");
    case Media::DisabledReason::AlreadyMounted: return QObject::tr("This device is already mounted.");
    case Media::DisabledReason::Gone: return QObject::tr("This attachment is no longer available.");
    case Media::DisabledReason::Stale: return QObject::tr("Refresh the current device information.");
    default: return QObject::tr("This action is unavailable for the current device.");
    }
}
QString rowState(const Media::VolumeRow &row) {
    if (row.progress != Media::ProgressPhase::Idle) return QObject::tr("Operation in progress");
    if (row.locked) return QObject::tr("Locked");
    if (row.mountState != Media::MountState::Mounted) return QObject::tr("Not mounted");
    return row.readOnly == Media::ReadOnlyState::ReadOnly ? QObject::tr("Mounted read-only")
         : row.readOnly == Media::ReadOnlyState::Writable ? QObject::tr("Mounted")
         : QObject::tr("Mounted; write permission unknown");
}
}
MediaPresenter::MediaPresenter(Media::MediaSource &source, FolderNavigations &navigations,
                               QObject *parent)
    : QObject(parent), m_source(source), m_navigations(navigations) {
    connect(&source, &Media::MediaSource::snapshotChanged, this, &MediaPresenter::sourceChanged);
    connect(&source, &Media::MediaSource::operationFinished, this, &MediaPresenter::finished);
    connect(&navigations, &FolderNavigations::activeChanged, this, [this] {
        // Changing tab retires only this window's deferred open, not owner work.
        m_openAfter.reset();
        observeActive();
        emit changed();
    });
    connect(&navigations, &FolderNavigations::controllerCreated, this, [this](QObject *created) {
        observeController(qobject_cast<NavigationController *>(created));
    });
    for (auto *navigation : navigations.controllers()) observeController(navigation);
    observeActive();
}
QVariantList MediaPresenter::rows() const {
    QVariantList result;
    const auto snapshot = m_source.snapshot();
    for (const auto &row : snapshot.rows) {
        QString label = row.displayName;
        if (row.partitionNumber) label += tr(" · partition %1").arg(row.partitionNumber);
        const bool canOpen = row.actions.open.enabled || row.actions.mount.enabled;
        result.append(QVariantMap{
            {QStringLiteral("handle"), row.attachment.handle},
            {QStringLiteral("name"), label}, {QStringLiteral("kind"), row.kind},
            {QStringLiteral("status"), rowState(row)},
            {QStringLiteral("openEnabled"), canOpen && !busy()},
            {QStringLiteral("openReason"), reason(row.mountState == Media::MountState::Mounted
                ? row.actions.open.reason : row.actions.mount.reason)},
            {QStringLiteral("readOnlyEnabled"), row.actions.mountReadOnly.enabled && !busy()},
            {QStringLiteral("readOnlyReason"), reason(row.actions.mountReadOnly.reason)},
            {QStringLiteral("unmountEnabled"), row.actions.unmount.enabled && !busy()},
            {QStringLiteral("unmountReason"), reason(row.actions.unmount.reason)},
            {QStringLiteral("removeEnabled"), row.actions.remove.enabled && !busy()},
            {QStringLiteral("removeReason"), reason(row.actions.remove.reason)},
            {QStringLiteral("detailsEnabled"), row.actions.showDetails.enabled && !busy()},
            {QStringLiteral("detailsReason"), reason(row.actions.showDetails.reason)}});
    }
    return result;
}
QString MediaPresenter::state() const {
    switch (m_source.snapshot().availability) {
    case Media::Availability::Ready: return QStringLiteral("ready");
    case Media::Availability::Loading: return QStringLiteral("loading");
    case Media::Availability::Unavailable: return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}
QString MediaPresenter::recoveryLabel() const {
    return m_source.ownerObserved() ? tr("Try again") : tr("Start Removable Media");
}
bool MediaPresenter::busy() const {
    return m_source.actionPending() || m_source.snapshot().pending.has_value();
}
void MediaPresenter::open(const QString &handle) {
    const auto snapshot = m_source.snapshot();
    const auto row = std::find_if(snapshot.rows.cbegin(), snapshot.rows.cend(),
        [&](const auto &value) { return value.attachment.handle == handle; });
    auto *navigation = qobject_cast<NavigationController *>(m_navigations.active());
    if (row == snapshot.rows.cend() || !navigation || snapshot.availability != Media::Availability::Ready) return;
    if (row->actions.open.enabled && row->mountState == Media::MountState::Mounted)
        openRow(*row, *navigation);
    else request(handle, Media::Action::Mount, true);
}
void MediaPresenter::request(const QString &handle, Media::Action action, bool openAfter) {
    if (busy()) return;
    const auto snapshot = m_source.snapshot();
    const auto row = std::find_if(snapshot.rows.cbegin(), snapshot.rows.cend(),
        [&](const auto &value) { return value.attachment.handle == handle; });
    if (row == snapshot.rows.cend() || snapshot.availability != Media::Availability::Ready) return;
    m_openAfter.reset();
    auto *navigation = qobject_cast<NavigationController *>(m_navigations.active());
    if (openAfter && navigation)
        m_openAfter = DeferredOpen{navigation, snapshot.lineage, row->attachment,
                                   navigation->currentPath(), navigation->listingGeneration()};
    m_requestId = m_source.requestAction(row->attachment, action);
    if (m_requestId.isEmpty()) {
        m_openAfter.reset();
        m_notice = tr("This device action is unavailable. Refresh and try again.");
    } else m_notice.clear();
    emit changed();
}
void MediaPresenter::mountReadOnly(const QString &handle) { request(handle, Media::Action::MountReadOnly, true); }
void MediaPresenter::unmount(const QString &handle) { request(handle, Media::Action::Unmount, false); }
void MediaPresenter::remove(const QString &handle) { request(handle, Media::Action::Remove, false); }
void MediaPresenter::details(const QString &handle) { request(handle, Media::Action::ShowDetails, false); }
void MediaPresenter::recover() { m_source.recover(); }
void MediaPresenter::refresh() { m_source.refresh(); }
void MediaPresenter::openOwner() { m_source.openOwner(); }
void MediaPresenter::observeActive() {
    observeController(qobject_cast<NavigationController *>(m_navigations.active()));
}
void MediaPresenter::observeController(NavigationController *navigation) {
    if (!navigation) return;
    // Per-controller provenance includes never-active panes. Window actions
    // still bind exclusively through FolderNavigations' active binder.
    connect(navigation, &NavigationController::navigationChanged, this, &MediaPresenter::navigationChanged, Qt::UniqueConnection);
    acquireLocation(*navigation);
}
void MediaPresenter::acquireLocation(NavigationController &navigation, bool deliberate) {
    if (navigation.mediaLocationRevoked()) return;
    const auto snapshot = m_source.snapshot();
    if (snapshot.availability != Media::Availability::Ready) return;
    const auto selected = locationAt(snapshot, navigation.currentPath());
    if (selected.ambiguous) {
        navigation.invalidateMediaLocation(tr("This mount location is ambiguous. Choose another folder or inspect Removable Media."));
        return;
    }
    if (!selected.row) return;
    auto existing = std::find_if(m_locations.begin(), m_locations.end(),
        [&](const auto &value) { return value.navigation == &navigation; });
    const LocationInterest interest{&navigation, snapshot.lineage, selected.row->attachment, selected.root, false};
    if (existing == m_locations.end()) m_locations.append(interest);
    else if (deliberate) *existing = interest;
}

void MediaPresenter::openRow(const Media::VolumeRow &row, NavigationController &navigation) {
    if (row.preferredRoot.isEmpty() || !row.mountRoots.contains(row.preferredRoot)) return;
    const auto selected = locationAt(m_source.snapshot(), row.preferredRoot);
    if (!selected.row || selected.ambiguous || selected.row->attachment != row.attachment) {
        m_notice = tr("This mount location is ambiguous. Refresh or choose another folder.");
        emit changed();
        return;
    }
    auto existing = std::find_if(m_locations.begin(), m_locations.end(),
        [&](const auto &value) { return value.navigation == &navigation; });
    const auto interest = LocationInterest{&navigation, m_source.snapshot().lineage,
                                          row.attachment, row.preferredRoot, false};
    if (existing == m_locations.end()) {
        m_locations.append(interest);
        connect(&navigation, &NavigationController::navigationChanged, this, &MediaPresenter::navigationChanged, Qt::UniqueConnection);
    } else *existing = interest;
    m_opening = true;
    navigation.navigateTo(row.preferredRoot);
    m_opening = false;
}
void MediaPresenter::navigationChanged() {
    if (m_opening || m_reconciling) return;
    m_locations.removeIf([](const auto &interest) {
        return !interest.navigation || !within(interest.navigation->currentPath(), interest.root)
            || (interest.revoked && !interest.navigation->mediaLocationRevoked());
    });
    if (m_openAfter && (!m_openAfter->navigation
        || m_openAfter->navigation->currentPath() != m_openAfter->path
        || m_openAfter->navigation->listingGeneration() != m_openAfter->generation))
        m_openAfter.reset();
    if (auto *navigation = qobject_cast<NavigationController *>(sender())) acquireLocation(*navigation, true);
}
void MediaPresenter::sourceChanged() {
    const auto snapshot = m_source.snapshot();
    m_reconciling = true;
    for (auto &interest : m_locations) {
        if (!interest.navigation || interest.revoked) continue;
        const auto *row = rowFor(snapshot, interest.attachment);
        const auto selected = locationAt(snapshot, interest.navigation->currentPath());
        if (snapshot.availability != Media::Availability::Ready
            || !sameOwner(snapshot.lineage, interest.lineage) || !row
            || row->mountState != Media::MountState::Mounted || !row->mountRoots.contains(interest.root)
            || selected.ambiguous || !selected.row || selected.row->attachment != interest.attachment
            || selected.root != interest.root) {
            // AGENT-GUARD: a mount pathname can be reused. Passive refresh,
            // owner replacement and later rows must never revive old authority.
            interest.revoked = true;
            interest.navigation->invalidateMediaLocation(tr("This device is unavailable. Choose a current device or another folder."));
        }
    }
    m_reconciling = false;
    m_locations.removeIf([](const auto &interest) { return !interest.navigation; });
    for (auto *navigation : m_navigations.controllers()) observeController(navigation);
    observeActive();
    if (snapshot.availability != Media::Availability::Ready) {
        m_inventoryNotice = snapshot.diagnostic.message; m_notice = m_inventoryNotice;
    } else {
        if (m_notice == m_inventoryNotice) m_notice.clear();
        m_inventoryNotice.clear();
    }
    emit changed();
}
void MediaPresenter::finished(const Media::OperationResult &result) {
    if (result.request.requestId != m_requestId) return;
    m_requestId.clear();
    const auto deferred = m_openAfter;
    m_openAfter.reset();
    m_notice = result.status == Media::OperationStatus::Applied
        && result.request.action == Media::Action::Remove && result.removalMode != Media::RemovalMode::None
        ? tr("Safe to unplug this device.") : result.diagnostic.message;
    if (deferred && deferred->navigation && deferred->navigation == m_navigations.active()
        && deferred->navigation->currentPath() == deferred->path
        && deferred->navigation->listingGeneration() == deferred->generation
        && result.status == Media::OperationStatus::Applied && result.confirmingRevision
        && result.request.attachment == deferred->attachment
        && sameOwner(result.request.lineage, deferred->lineage)) {
        const auto snapshot = m_source.snapshot();
        const auto *row = rowFor(snapshot, deferred->attachment);
        if (snapshot.availability == Media::Availability::Ready && row
            && sameOwner(snapshot.lineage, deferred->lineage)
            && snapshot.lineage.revision >= *result.confirmingRevision
            && row->mountState == Media::MountState::Mounted)
            openRow(*row, *deferred->navigation);
    }
    emit changed();
}
}
