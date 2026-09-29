// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/application_window_management/client.h>
#include <qindaqt-window-management-v1-client-protocol.h>
#include <QGuiApplication>
#include <QWindow>
#include <cstring>
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QindaQt::ApplicationWindowManagement::WindowPlacementClient client;
    QWindow source,created;
    QString error;
    if(client.available() || client.place(&source,&created,QindaQt::ApplicationWindowManagement::Placement::Tab,&error)!=0 || error.isEmpty()) return 1;
    if(std::strcmp(qindaqt_window_manager_v1_interface.name,"qindaqt_window_manager_v1") || qindaqt_window_manager_v1_interface.version!=1) return 2;
    return 0;
}
