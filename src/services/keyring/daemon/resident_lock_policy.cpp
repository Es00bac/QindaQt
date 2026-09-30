// SPDX-License-Identifier: GPL-3.0-or-later
#include "resident_lock_policy.h"
#include "lock_policy.h"
#include "session_display_binding.h"
#include "secret_service.h"
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
namespace qindaqt::keyring::service {
namespace sc=QindaQt::Services::SettingsClient;
namespace sl=QindaQt::Services::SessionLockState;
namespace {
class NativeScreenObservation final : public LockObservation {
public:
    NativeScreenObservation(SessionDisplayBinding &display,QDBusConnection bus)
        :transport(std::move(bus)),monitor(transport,[&display](const QString &owner,quint64 pid){
            return !display.basename().isEmpty() && display.live()
                && owner==display.compositorOwner() && pid==display.compositorPid();
        }) {
        connect(&monitor,&sl::NativeLockStateMonitor::stateChanged,this,[this]{emit changed();});
        connect(&display,&SessionDisplayBinding::attached,this,[this]{monitor.stop();(void)monitor.start();});
        connect(&display,&SessionDisplayBinding::revoked,this,[this]{monitor.stop();emit changed();});
        if(!display.basename().isEmpty()) (void)monitor.start();
    }
    sl::LockState state() const override{return monitor.state();}
private:
    sl::QtNativeLockTransport transport;
    sl::NativeLockStateMonitor monitor;
};
}
class ResidentLockPolicy::Private {
public:
    Private(ResidentLockPolicy &context,CollectionRepository &repository,SecretService &service,
        SessionDisplayBinding &display,QDBusConnection bus)
        :transport(bus),settings(transport,{"keyring.lockOnScreenLock","keyring.lockAfterIdleMinutes"}),
         screen(display,bus),idle([&display]{return display.openPromptConnection();},
            [&display]{return !display.basename().isEmpty() && display.live();}),
         policy(settings,screen,idle,[&repository,&service]{
            for(const auto &id:repository.names()) if(!repository.locked(id)) {
                repository.lock(id);service.notifyCollectionState(id);
            }
         }) {
        service.observeLockPolicy(&policy);
        QObject::connect(&display,&SessionDisplayBinding::attached,&context,[this]{idle.refresh();});
        QObject::connect(&display,&SessionDisplayBinding::revoked,&context,[this]{idle.revoke();});
        (void)settings.start();
    }
    sc::QtSettingsTransport transport;
    sc::SettingsClient settings;
    NativeScreenObservation screen;
    WaylandIdleObservation idle;
    KeyringLockPolicy policy;
};
ResidentLockPolicy::ResidentLockPolicy(CollectionRepository &repository,SecretService &service,
    SessionDisplayBinding &display,QDBusConnection bus,QObject *parent)
    :QObject(parent),d(std::make_unique<Private>(*this,repository,service,display,std::move(bus))) {}
ResidentLockPolicy::~ResidentLockPolicy()=default;
}
