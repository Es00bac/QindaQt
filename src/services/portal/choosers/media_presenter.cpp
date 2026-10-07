// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_presenter.h"
#include <QDir>
#include <algorithm>
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
const Media::VolumeRow *rowFor(const Media::Snapshot &s, const Media::Attachment &attachment) {
    const auto row = std::find_if(s.rows.cbegin(), s.rows.cend(),
        [&](const auto &v) { return v.attachment == attachment; });
    return row == s.rows.cend() ? nullptr : &*row;
}
}
ChooserMediaPresenter::ChooserMediaPresenter(Media::MediaSource &source, bool saving, QObject *parent)
    : QObject(parent), m_source(source), m_saving(saving) {
    connect(&source, &Media::MediaSource::snapshotChanged, this, &ChooserMediaPresenter::sourceChanged);
    connect(&source, &Media::MediaSource::operationFinished, this, &ChooserMediaPresenter::finished);
}
bool ChooserMediaPresenter::busy() const {
    return m_source.actionPending() || m_source.snapshot().pending.has_value();
}
QString ChooserMediaPresenter::restriction() const {
    if (m_revoked) return tr("This device is unavailable. Choose a current device or another folder.");
    if (m_readOnlySave) return tr("This device is read-only. Choose another folder to save.");
    return {};
}
QString ChooserMediaPresenter::recoveryLabel() const {
    return m_source.ownerObserved() ? tr("Try again") : tr("Start Removable Media");
}
void ChooserMediaPresenter::setLocation(const QString &path, bool deliberate) {
    if (m_closed || (!deliberate && m_revoked)) return;
    if (deliberate) { m_revoked = false; m_openAfter.reset(); }
    m_path = path; m_location.reset(); m_readOnlySave = false;
    const auto current = m_source.snapshot();
    if (current.availability == Media::Availability::Ready) {
        const auto selected = locationAt(current, path);
        if (selected.ambiguous) {
            m_revoked = true;
            m_notice = tr("This mount location is ambiguous. Choose another folder or inspect Removable Media.");
            emit locationInvalidated();
        } else if (selected.row) {
            m_location = Interest{current.lineage, selected.row->attachment, selected.root};
            m_readOnlySave = m_saving && selected.row->readOnly == Media::ReadOnlyState::ReadOnly;
        }
    }
    emit changed();
}
void ChooserMediaPresenter::open(const QString &handle) {
    const auto current = m_source.snapshot();
    const auto row = std::find_if(current.rows.cbegin(), current.rows.cend(),
        [&](const auto &v) { return v.attachment.handle == handle; });
    if (m_closed || row == current.rows.cend() || current.availability != Media::Availability::Ready) return;
    if (row->actions.open.enabled && row->mountState == Media::MountState::Mounted) openRow(*row);
    else request(handle, Media::Action::Mount, true);
}
void ChooserMediaPresenter::request(const QString &handle, Media::Action action, bool openAfter) {
    if (m_closed || busy()) return;
    const auto current = m_source.snapshot();
    const auto row = std::find_if(current.rows.cbegin(), current.rows.cend(),
        [&](const auto &v) { return v.attachment.handle == handle; });
    if (row == current.rows.cend() || current.availability != Media::Availability::Ready) return;
    m_openAfter.reset();
    if (openAfter) { m_openAfter = Interest{current.lineage, row->attachment, {}}; m_openPath = m_path; }
    m_requestId = m_source.requestAction(row->attachment, action);
    if (m_requestId.isEmpty()) { m_openAfter.reset(); m_notice = tr("This device action is unavailable. Refresh and try again."); }
    else m_notice.clear();
    emit changed();
}
void ChooserMediaPresenter::openRow(const Media::VolumeRow &row) {
    if (row.preferredRoot.isEmpty() || !row.mountRoots.contains(row.preferredRoot)) return;
    const auto selected = locationAt(m_source.snapshot(), row.preferredRoot);
    if (!selected.row || selected.ambiguous || selected.row->attachment != row.attachment) {
        m_notice = tr("This mount location is ambiguous. Refresh or choose another folder.");
        emit changed(); return;
    }
    emit navigateRequested(row.preferredRoot);
}
void ChooserMediaPresenter::recover() { if (!m_closed) m_source.recover(); }
void ChooserMediaPresenter::openOwner() { if (!m_closed) m_source.openOwner(); }
void ChooserMediaPresenter::closeInterest() { m_closed = true; m_openAfter.reset(); m_location.reset(); }
void ChooserMediaPresenter::sourceChanged() {
    if (m_closed) return;
    const auto current = m_source.snapshot();
    if (m_location && !m_revoked) {
        const auto *row = rowFor(current, m_location->attachment);
        const auto selected = locationAt(current, m_path);
        if (current.availability != Media::Availability::Ready || !sameOwner(current.lineage, m_location->lineage)
            || !row || row->mountState != Media::MountState::Mounted || !row->mountRoots.contains(m_location->root)
            || selected.ambiguous || !selected.row || selected.row->attachment != m_location->attachment
            || selected.root != m_location->root) {
            m_revoked = true; m_readOnlySave = false; m_openAfter.reset(); emit locationInvalidated();
        } else m_readOnlySave = m_saving && row->readOnly == Media::ReadOnlyState::ReadOnly;
    } else if (!m_location && !m_revoked && !m_path.isEmpty()) {
        // AGENT-GUARD: first observation may classify an ordinary folder;
        // owner recovery must never revive a previously revoked selection.
        setLocation(m_path, false);
    }
    if (current.availability != Media::Availability::Ready) {
        m_inventoryNotice = current.diagnostic.message; m_notice = m_inventoryNotice;
    } else {
        if (m_notice == m_inventoryNotice) m_notice.clear();
        m_inventoryNotice.clear();
    }
    emit changed();
}
void ChooserMediaPresenter::finished(const Media::OperationResult &result) {
    if (m_closed || result.request.requestId != m_requestId) return;
    m_requestId.clear(); const auto deferred = m_openAfter; m_openAfter.reset();
    m_notice = result.status == Media::OperationStatus::Applied && result.request.action == Media::Action::Remove
        && result.removalMode != Media::RemovalMode::None ? tr("Safe to unplug this device.") : result.diagnostic.message;
    if (deferred && m_path == m_openPath && result.status == Media::OperationStatus::Applied
        && result.confirmingRevision && result.request.attachment == deferred->attachment
        && sameOwner(result.request.lineage, deferred->lineage)) {
        const auto current = m_source.snapshot(); const auto *row = rowFor(current, deferred->attachment);
        if (current.availability == Media::Availability::Ready && row && sameOwner(current.lineage, deferred->lineage)
            && current.lineage.revision >= *result.confirmingRevision && row->mountState == Media::MountState::Mounted)
            openRow(*row);
    }
    emit changed();
}
