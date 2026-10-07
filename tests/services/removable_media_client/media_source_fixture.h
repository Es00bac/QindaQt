// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/media_source.h>
#include <QUuid>
#include <algorithm>
namespace MediaFixture {
namespace Media = QindaQt::RemovableMedia;
inline void setMounted(Media::VolumeRow &row, const QString &root, Media::ReadOnlyState ro = Media::ReadOnlyState::Writable) {
    const bool mounted = !root.isEmpty();
    row.mountState = mounted ? Media::MountState::Mounted : Media::MountState::Unmounted;
    row.mountRoots = mounted ? QStringList{root} : QStringList{};
    row.preferredRoot = root; row.readOnly = ro;
    row.actions.open = {mounted, mounted ? Media::DisabledReason::None : Media::DisabledReason::NotMounted};
    row.actions.mount = {!mounted, mounted ? Media::DisabledReason::AlreadyMounted : Media::DisabledReason::None};
    row.actions.mountReadOnly = row.actions.mount;
    row.actions.unmount = row.actions.open;
    row.actions.remove = {true, Media::DisabledReason::None};
    row.actions.showDetails = {true, Media::DisabledReason::None};
}
inline Media::VolumeRow volume(const QString &handle, const QString &root = {}) {
    Media::VolumeRow row;
    row.driveDisplayId = QStringLiteral("drive_") + handle;
    row.volumeDisplayId = QStringLiteral("volume_") + handle;
    row.attachment = {handle, 1}; row.displayName = QStringLiteral("<b>Shared label</b>");
    row.kind = QStringLiteral("Filesystem"); row.partitionNumber = 1;
    setMounted(row, root); return row;
}
class Source final : public Media::MediaSource {
public:
    Source() { value.lineage = {QStringLiteral(":1.999"), QStringLiteral("fixture_epoch"), 1}; value.availability = Media::Availability::Ready; }
    Media::Snapshot snapshot() const override { return value; }
    void start() override { ++starts; }
    void refresh() override { ++refreshes; }
    void recover() override { ++recoveries; }
    void openOwner() override { ++ownerOpens; }
    bool actionPending() const override { return pending.has_value(); }
    QString requestAction(const Media::Attachment &attachment, Media::Action action) override {
        if (pending || value.availability != Media::Availability::Ready) return {};
        const auto row = std::find_if(value.rows.cbegin(), value.rows.cend(), [&](const auto &v) { return v.attachment == attachment; });
        if (row == value.rows.cend()) return {};
        const auto &available = action == Media::Action::Mount ? row->actions.mount
            : action == Media::Action::MountReadOnly ? row->actions.mountReadOnly
            : action == Media::Action::Unmount ? row->actions.unmount
            : action == Media::Action::Remove ? row->actions.remove : row->actions.showDetails;
        if (!available.enabled) return {};
        Media::ActionRequest request; request.requestId = QStringLiteral("request_%1").arg(++writes);
        request.lineage = value.lineage; request.attachment = attachment; request.action = action;
        pending = request; requests.append(request); return request.requestId;
    }
    void publish() { emit snapshotChanged(); }
    void finish(Media::OperationStatus status = Media::OperationStatus::Applied,
                Media::RemovalMode mode = Media::RemovalMode::None) {
        if (!pending) return;
        Media::OperationResult result; result.request = *pending;
        result.operationId = QStringLiteral("operation_") + pending->requestId;
        result.status = status; result.removalMode = mode;
        if (status == Media::OperationStatus::Applied && pending->action != Media::Action::Remove
            && pending->action != Media::Action::ShowDetails) result.confirmingRevision = value.lineage.revision;
        pending.reset(); emit operationFinished(result);
    }
    Media::Snapshot value;
    std::optional<Media::ActionRequest> pending;
    QList<Media::ActionRequest> requests;
    int starts = 0, refreshes = 0, recoveries = 0, ownerOpens = 0, writes = 0;
};
}
