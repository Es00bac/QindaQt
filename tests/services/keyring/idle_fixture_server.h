// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ext-idle-notify-server.h"
#include <QSocketNotifier>
#include <wayland-server.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
class Server {
public:
    explicit Server(int count=1,bool protocol=true){
        display=wl_display_create();loop=wl_display_get_event_loop(display);
        if(protocol) manager=wl_global_create(display,&ext_idle_notifier_v1_interface,1,this,
            [](wl_client *client,void *data,uint32_t version,uint32_t id){
                auto *resource=wl_resource_create(client,&ext_idle_notifier_v1_interface,static_cast<int>(version),id);
                static const struct ext_idle_notifier_v1_interface operations{
                    [](wl_client *,wl_resource *r){wl_resource_destroy(r);},
                    [](wl_client *client,wl_resource *r,uint32_t id,uint32_t timeout,wl_resource *){
                        auto &self=*static_cast<Server *>(wl_resource_get_user_data(r));self.timeout=timeout;
                        auto *notification=wl_resource_create(client,&ext_idle_notification_v1_interface,1,id);
                        static const struct ext_idle_notification_v1_interface destroy{
                            [](wl_client *,wl_resource *value){wl_resource_destroy(value);}
                        };
                        wl_resource_set_implementation(notification,&destroy,&self,[](wl_resource *value){
                            auto &server=*static_cast<Server *>(wl_resource_get_user_data(value));
                            if(server.notification==value) server.notification=nullptr;
                        });self.notification=notification;
                    },nullptr
                };
                wl_resource_set_implementation(resource,&operations,data,nullptr);
            });
        for(int i=0;i<count;++i) wl_global_create(display,&wl_seat_interface,1,this,
            [](wl_client *client,void *,uint32_t version,uint32_t id){
                auto *seat=wl_resource_create(client,&wl_seat_interface,static_cast<int>(version),id);
                wl_resource_set_implementation(seat,nullptr,nullptr,nullptr);
                wl_seat_send_capabilities(seat,0);
            });
        watcher=std::make_unique<QSocketNotifier>(wl_event_loop_get_fd(loop),QSocketNotifier::Read);
        QObject::connect(watcher.get(),&QSocketNotifier::activated,watcher.get(),[this]{wl_event_loop_dispatch(loop,0);wl_display_flush_clients(display);});
    }
    ~Server(){watcher.reset();wl_display_destroy_clients(display);wl_display_destroy(display);}
    int connection(){
        int pair[2];if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair)!=0) return -1;
        if(!wl_client_create(display,pair[0])) {close(pair[0]);close(pair[1]);return -1;}return pair[1];
    }
    void idled(){ext_idle_notification_v1_send_idled(notification);wl_display_flush_clients(display);}
    void resumed(){ext_idle_notification_v1_send_resumed(notification);wl_display_flush_clients(display);}
    wl_display *display=nullptr;wl_event_loop *loop=nullptr;wl_global *manager=nullptr;
    wl_resource *notification=nullptr;uint32_t timeout=0;
    std::unique_ptr<QSocketNotifier> watcher;
};
