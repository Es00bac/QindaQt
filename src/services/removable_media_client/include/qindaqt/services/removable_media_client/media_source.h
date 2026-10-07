// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_types.h>
#include <QObject>

namespace QindaQt::RemovableMedia {
// GUI-thread boundary borrowed by presenters. Dependencies outlive borrowers;
// snapshot() returns an owning copy. start()/refresh() observe only an existing
// owner: neither starts the insertion helper nor mounts storage. Recovery is
// deliberate user input. Destruction withdraws only this consumer's interest.
class MediaSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual Snapshot snapshot() const = 0;
    virtual void start() = 0;
    virtual void refresh() = 0;
    virtual void recover() = 0;
    virtual void openOwner() = 0;
    [[nodiscard]] virtual bool ownerObserved() const { return !snapshot().lineage.owner.isEmpty(); }
    // Deliberate closed action; returns a new correlation id or empty refusal.
    // No accepted action is cancelled or replayed by client destruction/refresh.
    [[nodiscard]] virtual bool actionPending() const { return false; }
    [[nodiscard]] virtual QString requestAction(const Attachment &, Action) { return {}; }
Q_SIGNALS:
    void snapshotChanged();
    void admissionReceived(const ActionAdmission &admission);
    void operationFinished(const OperationResult &result);
};

// Closed launch port: implementations resolve only the installed
// org.qindaqt.RemovableMedia.desktop entry through the application-launch
// boundary. A successful launch is not inventory readiness; the client still
// waits for observed owner + validated snapshot. No executable/path parameter.
class MediaOwnerLauncher {
public:
    virtual ~MediaOwnerLauncher() = default;
    [[nodiscard]] virtual bool startOwner() = 0;
};
} // namespace QindaQt::RemovableMedia

Q_DECLARE_METATYPE(QindaQt::RemovableMedia::ActionAdmission)
Q_DECLARE_METATYPE(QindaQt::RemovableMedia::OperationResult)
