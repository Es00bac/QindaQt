// SPDX-License-Identifier: GPL-3.0-or-later
#include "collector_process_p.h"
#include "collector_recipe_p.h"
#include "protected_collector_file_p.h"
#include "qindaqt/services/android_runtime/native_bootstrap.h"
#include "qindaqt/android_manager/application_controller.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <fcntl.h>
using namespace QindaQt::Services::AndroidRuntime;
class NativeBootstrapTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void packagedRecipeMatchesCompiledPolicy();
    void protectedFilesRejectWritableStaging();
    void missingProductionContextDoesNotInjectPort();
    void actualHelperPrivateAdmissionStaysUnavailable();
    void contextLossFencesBorrowedManagerBeforeNotification();
    void helperRefusesPublicArgumentMode();
};
void NativeBootstrapTest::packagedRecipeMatchesCompiledPolicy() {
    QFile recipe(QString::fromUtf8(ANDROID_COLLECTOR_RECIPE)); QVERIFY(recipe.open(QIODevice::ReadOnly));
    QCOMPARE(recipe.readAll(),collectorRecipe());
    const auto object=QJsonDocument::fromJson(collectorRecipe()).object();
    QCOMPARE(object.value(QStringLiteral("runtime")).toString(),QStringLiteral("waydroid-stock"));
    QVERIFY(!object.value(QStringLiteral("guestQualified")).toBool());
}
void NativeBootstrapTest::protectedFilesRejectWritableStaging() {
    QTemporaryDir staging; QVERIFY(staging.isValid());
    QFile record(staging.filePath(QStringLiteral("collector.json"))); QVERIFY(record.open(QIODevice::WriteOnly));
    QCOMPARE(record.write(collectorRecipe()),collectorRecipe().size()); record.close();
    ProtectedCollectorFile untrusted(record.fileName(),false); QVERIFY(!untrusted.live());
    QCOMPARE(untrusted.duplicate(),-1);
}
void NativeBootstrapTest::missingProductionContextDoesNotInjectPort() {
    NativeRuntimeBootstrap native; QVERIFY(!native.start()); QVERIFY(!native.port()); QVERIFY(!native.error().isEmpty());
    QindaQt::AndroidManager::ApplicationController manager(nullptr,nullptr,100,nullptr,native.error());
    QCOMPARE(manager.runtimeState(),QStringLiteral("unavailable")); QVERIFY(manager.applications().isEmpty());
    QCOMPARE(manager.runtimeMessage(),native.error());
}
void NativeBootstrapTest::actualHelperPrivateAdmissionStaysUnavailable() {
    // Owning-module fixture provides an explicitly admitted build executable;
    // production fixed root-file and Session1 policy is tested separately.
    CollectorProcess process([]{return true;});
    QVERIFY(process.start(::open(ANDROID_COLLECTOR_EXE,O_RDONLY|O_CLOEXEC)));
    QVERIFY(process.admitted()); const auto value=process.snapshot();
    QCOMPARE(value.state,RuntimeState::Unavailable); QCOMPARE(value.revision,quint64(1));
    QVERIFY(!value.attachmentId.isEmpty()); QVERIFY(value.registrations.isEmpty()); QVERIFY(value.applications.isEmpty());
    QindaQt::AndroidManager::ApplicationController manager(&process);
    QCOMPARE(manager.runtimeState(),QStringLiteral("unavailable")); QVERIFY(manager.applications().isEmpty());
    process.requestRefresh();
    // Unchanged copied observations intentionally deduplicate without a signal.
    QTest::qWait(20); QVERIFY(process.admitted());
    QCOMPARE(process.snapshot(),value); QVERIFY(!process.requestLaunch({}).accepted);
}
void NativeBootstrapTest::contextLossFencesBorrowedManagerBeforeNotification() {
    bool live=true; CollectorProcess process([&]{return live;});
    QVERIFY(process.start(::open(ANDROID_COLLECTOR_EXE,O_RDONLY|O_CLOEXEC)));
    QindaQt::AndroidManager::ApplicationController manager(&process);
    live=false; QVERIFY(!process.admitted()); QVERIFY(process.snapshot().attachmentId.isEmpty());
    QVERIFY(!process.requestLaunch({}).accepted);
    QVERIFY(manager.refresh()); // exact public port getter performs the context fence
    QVERIFY(manager.applications().isEmpty()); QCOMPARE(manager.runtimeState(),QStringLiteral("unavailable"));
}
void NativeBootstrapTest::helperRefusesPublicArgumentMode() {
    QProcess process; process.setProgram(QString::fromUtf8(ANDROID_COLLECTOR_EXE));
    process.setArguments({QStringLiteral("--public-fd=3")}); process.start();
    QVERIFY(process.waitForFinished(1000)); QCOMPARE(process.exitCode(),2);
}
QTEST_GUILESS_MAIN(NativeBootstrapTest)
#include "tst_native_bootstrap.moc"
