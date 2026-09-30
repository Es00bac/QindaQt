// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <memory>
namespace qindaqt::keyring::service {
class CollectionRepository;
class SecretService;
class SessionDisplayBinding;
// QCore-only production composition. Borrows same-thread repository, service,
// binding and connection until destruction; all outlive it. The native lock
// port admits only binding's current unique owner/PID/ordinary-peer lineage.
// No display-env guess, legacy lock quorum or local activity idle heuristic.
class ResidentLockPolicy final : public QObject {
    Q_OBJECT
public:
    ResidentLockPolicy(CollectionRepository &,SecretService &,SessionDisplayBinding &,
                       QDBusConnection,QObject *parent=nullptr);
    ~ResidentLockPolicy() override;
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
