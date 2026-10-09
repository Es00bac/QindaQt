// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/android_runtime/native_bootstrap.h"
#include "qindaqt/services/android_runtime/renderer_lifetime.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include "collector_process_p.h"
#include "collector_recipe_p.h"
#include "protected_collector_file_p.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <sys/stat.h>
#include <unistd.h>
namespace QindaQt::Services::AndroidRuntime {
class NativeRuntimeBootstrap::Private {
public:
    ProtectedCollectorFile helper{QStringLiteral("/usr/libexec/qindaqt/qindaqt-android-collector"),true};
    ProtectedCollectorFile recipe{QStringLiteral("/usr/share/qindaqt/android-runtime/collector.json"),false};
    ProtectedCollectorFile sessionCode{QStringLiteral("/usr/bin/qindaqt-session"),true};
    QDBusConnection bus=QDBusConnection::sessionBus();
    QString owner,busId,failure;
    unsigned pid=0;
    std::unique_ptr<HeldRendererLifetime> session;
    // No guest renderer is admitted or held when guest-qualified facts are absent.
    std::unique_ptr<CollectorProcess> collector;
    bool live() const {
        if (!helper.live() || !recipe.live() || recipe.bytes()!=collectorRecipe() || !sessionCode.live()
            || !session || !session->processId() || !bus.isConnected() || !bus.interface()) return false;
        const QDBusReply<QString> currentId=bus.interface()->call(QStringLiteral("GetId"));
        const auto current=bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
        const auto currentPid=bus.interface()->servicePid(owner); const auto uid=bus.interface()->serviceUid(owner);
        return currentId.isValid() && currentId.value()==busId && current.isValid() && current.value()==owner
            && currentPid.isValid() && currentPid.value()==pid && uid.isValid() && uid.value()==::getuid()
            && session->processId().has_value();
    }
};
NativeRuntimeBootstrap::NativeRuntimeBootstrap():d(std::make_unique<Private>()) {}
NativeRuntimeBootstrap::~NativeRuntimeBootstrap()=default;
bool NativeRuntimeBootstrap::start() {
    if (d->collector) return d->collector->admitted(); // no restart/replay loop
    if (!d->helper.live() || !d->recipe.live() || d->recipe.bytes()!=collectorRecipe()) {
        d->failure=QStringLiteral("The package-owned Android collector is unavailable. Prepare Android through Software Manager."); return false;
    }
    if (!d->sessionCode.live() || !d->bus.isConnected() || !d->bus.interface()) {
        d->failure=QStringLiteral("The supervised QindaQt session is unavailable. Open Android Applications from your QindaQt session."); return false;
    }
    d->failure=QStringLiteral("QindaQt session identity could not be admitted.");
    const QDBusReply<QString> busId=d->bus.interface()->call(QStringLiteral("GetId"));
    const auto owner=d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    if (!busId.isValid() || !owner.isValid() || owner.value().isEmpty()) return false;
    const auto pid=d->bus.interface()->servicePid(owner.value()); const auto uid=d->bus.interface()->serviceUid(owner.value());
    if (!pid.isValid() || pid.value()<=1 || !uid.isValid() || uid.value()!=::getuid()) return false;
    const int code=d->sessionCode.duplicate(); if (code<0) return false;
    d->owner=owner.value(); d->pid=pid.value(); d->busId=busId.value();
    d->session=std::make_unique<HeldRendererLifetime>(holdProducerLifetime(static_cast<int>(d->pid)),code,static_cast<int>(d->pid));
    if (!d->live()) { d->failure=QStringLiteral("QindaQt session identity could not be admitted."); return false; }
    d->collector=std::make_unique<CollectorProcess>([this]{ return d->live(); });
    if (!d->collector->start(d->helper.duplicate())) {
        d->failure=QStringLiteral("The Android collector could not establish its private session connection."); return false;
    }
    d->failure.clear(); return true;
}
RuntimePort *NativeRuntimeBootstrap::port() const { return d->collector && d->collector->admitted()?d->collector.get():nullptr; }
QString NativeRuntimeBootstrap::error() const { return d->failure; }
} // namespace QindaQt::Services::AndroidRuntime
