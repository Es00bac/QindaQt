// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <functional>
#include <memory>
namespace qindaqt::keyring::service {
// Thread-confined true compositor idle observation seam. No local timer can
// manufacture idle. Availability loss clears the observed state, never unlocks.
class IdleObservation : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void setTimeout(int milliseconds)=0;
    virtual bool available() const=0;
    virtual bool idle() const=0;
Q_SIGNALS:
    void changed();
};
// Owns each nonnegative connected ordinary display FD returned by opener.
// Opener and lineageLive are borrowed same-thread policy and must validate current owner/peer
// lineage; revoke() must run on lineage loss. No pathname reconnect or locker
// FD. Supports exactly one advertised wl_seat; ambiguous/missing protocol
// fails unavailable. This QCore-only seam is reusable by future power policy.
class WaylandIdleObservation final : public IdleObservation {
    Q_OBJECT
public:
    explicit WaylandIdleObservation(std::function<int()> opener,std::function<bool()> lineageLive,QObject *parent=nullptr);
    ~WaylandIdleObservation() override;
    void setTimeout(int) override;
    bool available() const override;
    bool idle() const override;
    void refresh();
    void revoke();
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
