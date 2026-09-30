// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_observer.h"
#include "ext-idle-notify-client.h"
#include <QSocketNotifier>
#include <QTimer>
#include <wayland-client.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
namespace qindaqt::keyring::service {
class WaylandIdleObservation::Private {
public:
    Private(WaylandIdleObservation &object,std::function<int()> acquire,std::function<bool()> admitted):q(object),opener(std::move(acquire)),lineageLive(std::move(admitted)) {
        deadline.setSingleShot(true);deadline.setInterval(2000);
        QObject::connect(&deadline,&QTimer::timeout,&q,[this]{clear();});
    }
    ~Private(){clear(false);}
    void clear(bool publish=true) {
        if(dispatching) {
            ready=false;isIdle=false;deadline.stop();
            const auto serial=generation;
            QTimer::singleShot(0,&q,[this,serial,publish]{if(serial==generation) clear(publish);});
            if(publish) emit q.changed();
            return;
        }
        ++generation;
        deadline.stop();reading.reset();writing.reset();
        if(notification) ext_idle_notification_v1_destroy(notification);
        notification=nullptr;
        if(sync) wl_callback_destroy(sync);
        sync=nullptr;
        if(manager) ext_idle_notifier_v1_destroy(manager);
        manager=nullptr;
        if(seat) wl_seat_destroy(seat);
        seat=nullptr;
        if(registry) wl_registry_destroy(registry);
        registry=nullptr;
        if(display) wl_display_disconnect(display);
        display=nullptr;seats=0;ready=false;isIdle=false;seatName=0;managerName=0;
        if(publish) emit q.changed();
    }
    void flush() {
        if(!display) return;
        if(wl_display_flush(display)<0) {
            if(errno!=EAGAIN) {clear();return;}
            writing->setEnabled(true);
        } else writing->setEnabled(false);
    }
    void dispatch() {
        if(!display) return;
        while(wl_display_prepare_read(display)!=0) {
            dispatching=true;const auto pending=wl_display_dispatch_pending(display);dispatching=false;
            if(pending<0 || (!ready && !sync)) {clear();return;}
        }
        if(wl_display_read_events(display)<0) {clear();return;}
        dispatching=true;const auto result=wl_display_dispatch_pending(display);dispatching=false;
        if(result<0) {clear();return;}
        flush();
    }
    void arm() {
        if(notification) ext_idle_notification_v1_destroy(notification);
        notification=nullptr;isIdle=false;
        if(!ready || !timeout || !manager || !seat) {emit q.changed();return;}
        notification=ext_idle_notifier_v1_get_idle_notification(manager,static_cast<uint32_t>(timeout),seat);
        static const ext_idle_notification_v1_listener events{
            [](void *data,ext_idle_notification_v1 *){auto &self=*static_cast<Private *>(data);self.isIdle=true;emit self.q.changed();},
            [](void *data,ext_idle_notification_v1 *){auto &self=*static_cast<Private *>(data);self.isIdle=false;emit self.q.changed();}
        };
        ext_idle_notification_v1_add_listener(notification,&events,this);flush();emit q.changed();
    }
    void open() {
        clear();
        if(!timeout) return;
        if(!lineageLive()) return;
        const int fd=opener();if(fd<0) return;
        display=wl_display_connect_to_fd(fd);
        if(!display) {close(fd);return;}
        registry=wl_display_get_registry(display);
        static const wl_registry_listener globals{
            [](void *data,wl_registry *r,uint32_t name,const char *interface,uint32_t version){
                auto &self=*static_cast<Private *>(data);(void)version;
                if(std::strcmp(interface,wl_seat_interface.name)==0) {
                    ++self.seats;
                    if(!self.seat) {self.seat=static_cast<wl_seat *>(wl_registry_bind(r,name,&wl_seat_interface,1));self.seatName=name;
                        static const wl_seat_listener ignored{[](void *,wl_seat *,uint32_t){},[](void *,wl_seat *,const char *){}};
                        wl_seat_add_listener(self.seat,&ignored,&self);
                    }
                } else if(std::strcmp(interface,ext_idle_notifier_v1_interface.name)==0 && !self.manager) {
                    self.manager=static_cast<ext_idle_notifier_v1 *>(wl_registry_bind(r,name,&ext_idle_notifier_v1_interface,1));self.managerName=name;
                }
            },
            [](void *data,wl_registry *,uint32_t name){
                auto &self=*static_cast<Private *>(data);
                if(name==self.seatName || name==self.managerName) {
                    // Destruction during dispatch is deferred until its callback
                    // returns; generation stays unavailable immediately.
                    self.ready=false;self.isIdle=false;emit self.q.changed();
                    const auto serial=self.generation;
                    QTimer::singleShot(0,&self.q,[&self,serial]{if(serial==self.generation) self.clear();});
                }
            }
        };
        wl_registry_add_listener(registry,&globals,this);
        sync=wl_display_sync(display);
        static const wl_callback_listener completed{
            [](void *data,wl_callback *callback,uint32_t){
                auto &self=*static_cast<Private *>(data);wl_callback_destroy(callback);self.sync=nullptr;self.deadline.stop();
                self.ready=self.seats==1 && self.manager && self.seat;
                self.arm();
            }
        };
        wl_callback_add_listener(sync,&completed,this);
        const int descriptor=wl_display_get_fd(display);
        reading=std::make_unique<QSocketNotifier>(descriptor,QSocketNotifier::Read,&q);
        writing=std::make_unique<QSocketNotifier>(descriptor,QSocketNotifier::Write,&q);
        writing->setEnabled(false);
        QObject::connect(reading.get(),&QSocketNotifier::activated,&q,[this]{dispatch();});
        QObject::connect(writing.get(),&QSocketNotifier::activated,&q,[this]{flush();});
        deadline.start();flush();
    }
    WaylandIdleObservation &q;
    std::function<int()> opener;
    std::function<bool()> lineageLive;
    QTimer deadline;
    std::unique_ptr<QSocketNotifier> reading,writing;
    wl_display *display=nullptr;
    wl_registry *registry=nullptr;
    wl_callback *sync=nullptr;
    wl_seat *seat=nullptr;
    ext_idle_notifier_v1 *manager=nullptr;
    ext_idle_notification_v1 *notification=nullptr;
    uint32_t seatName=0,managerName=0;
    int seats=0,timeout=0;
    bool ready=false,isIdle=false,dispatching=false;
    quint64 generation=0;
};
WaylandIdleObservation::WaylandIdleObservation(std::function<int()> opener,std::function<bool()> lineageLive,QObject *parent):IdleObservation(parent),d(std::make_unique<Private>(*this,std::move(opener),std::move(lineageLive))) {}
WaylandIdleObservation::~WaylandIdleObservation()=default;
void WaylandIdleObservation::setTimeout(int milliseconds){
    if(milliseconds<0 || milliseconds>1440*60000) milliseconds=0;
    if(d->timeout==milliseconds) return;
    d->timeout=milliseconds;
    if(!milliseconds) d->clear();
    else if(d->display) d->arm();
    else d->open();
}
bool WaylandIdleObservation::available() const{return d->ready && d->lineageLive();}
bool WaylandIdleObservation::idle() const{return available() && d->isIdle;}
void WaylandIdleObservation::refresh(){d->open();}
void WaylandIdleObservation::revoke(){d->clear();}
}
