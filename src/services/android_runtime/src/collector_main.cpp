// SPDX-License-Identifier: GPL-3.0-or-later
#include "collector_recipe_p.h"
#include "qindaqt/services/android_runtime/producer_endpoint.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <fcntl.h>
#include <unistd.h>
using namespace QindaQt::Services::AndroidRuntime;
int main(int argc,char **argv) {
    QCoreApplication application(argc,argv);
    const int seals=::fcntl(collectorRecord,F_GET_SEALS);
    if (argc!=1 || seals<0 || (seals&(F_SEAL_SEAL|F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_WRITE))
        !=(F_SEAL_SEAL|F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_WRITE)) return 2;
    QFile record; if (!record.open(collectorRecord,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) return 2;
    const auto bytes=record.read(4097); if (bytes.size()>4096) return 2;
    const auto object=QJsonDocument::fromJson(bytes).object();
    const int parent=object.value(QStringLiteral("parentPid")).toInt();
    const auto keyHex=object.value(QStringLiteral("key")).toString().toLatin1();
    const auto runtime=object.value(QStringLiteral("runtime")).toString();
    if (object.size()!=5 || object.value(QStringLiteral("version")).toInt()!=1 || parent<=1 || parent!=::getppid()
        || keyHex.size()!=64 || QByteArray::fromHex(keyHex).toHex()!=keyHex
        || object.value(QStringLiteral("recipe")).toString().toLatin1()!=collectorRecipeDigest()
        || QUuid(runtime).isNull() || QUuid(runtime).toString(QUuid::WithoutBraces)!=runtime) return 2;
    ::close(collectorExecutable);
    ProducerEndpoint endpoint(collectorSocket,parentLifetime,parentExecutable,parent,::getuid(),QByteArray::fromHex(keyHex));
    if (!endpoint.live()) return 2;
    RuntimeSnapshot status; status.attachmentId=runtime; status.revision=1; status.state=RuntimeState::Unavailable;
    // AGENT-GUARD: admitted native transport is never qualified guest truth.
    // No image/package/task/renderer is provided by this helper version.
    status.message=QStringLiteral("Android guest images and the app collector are not qualified. Prepare the runtime and images in Software Manager.");
    QObject::connect(&endpoint,&ProducerEndpoint::refreshRequested,&application,[&]{
        if (!endpoint.publishSnapshot(status)) application.quit();
    });
    QObject::connect(&endpoint,&ProducerEndpoint::disconnected,&application,&QCoreApplication::quit);
    if (!endpoint.publishSnapshot(status)) return 2;
    return application.exec();
}
