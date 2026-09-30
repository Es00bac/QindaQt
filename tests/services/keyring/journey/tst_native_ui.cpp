// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/prctl.h>
#include <sys/resource.h>
#include "tests/services/keyring/fixtures.h"
#include <qindaqt/apps/settings_keyring/keyring_settings_model.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>
#include <QQmlComponent>
#include <QClipboard>
#include <QMimeData>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QProcess>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest/QTest>
#include <csignal>
#include <memory>
using QindaQt::Apps::SettingsKeyring::KeyringSettingsModel;
Q_IMPORT_QML_PLUGIN(QindaQtSettingsKeyringPlugin)
namespace {
QQuickItem *findVisual(QQuickItem *root,const QString &name,const QString &text={}) {
    if(!root) return nullptr;
    if((!name.isEmpty() && root->objectName()==name)
       || (!text.isEmpty() && root->property("text").toString()==text && root->isVisible())) return root;
    for(auto *child:root->childItems()) if(auto *found=findVisual(child,name,text)) return found;
    return nullptr;
}
}
class NativeUiJourney final:public QObject {
    Q_OBJECT
private:
    QQmlApplicationEngine engine;
    QQuickWindow *window=nullptr;
    KeyringSettingsModel *model=nullptr;
    QProcess replacement;
    bool click(const QString &text,const QString &name={}) {
        auto *control=findVisual(window->contentItem(),name,text);
        if(!control || !control->isEnabled()) return false;
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
            control->mapToScene(QPointF(control->width()/2,control->height()/2)).toPoint());return true;
    }
    bool clipboard(const char *expected) {
        QProcess probe;auto env=QProcessEnvironment::systemEnvironment();
        env.insert("QINDAQT_JOURNEY_CLIPBOARD_EXPECT",QString::fromLatin1(expected));
        probe.setProcessEnvironment(env);probe.start(qEnvironmentVariable("QINDAQT_JOURNEY_CLIPBOARD"));
        QSignalSpy finished(&probe,&QProcess::finished);
        if(probe.state()!=QProcess::NotRunning && !finished.wait(6000)) {probe.kill();probe.waitForFinished();return false;}
        return probe.exitStatus()==QProcess::NormalExit && probe.exitCode()==0;
    }
    bool clipboardBytesEmpty() {
        QString text=QGuiApplication::clipboard()->text();const bool empty=text.isEmpty();
        text.fill(QChar(0));text.clear();return empty;
    }
    bool attach() {
        auto message=QDBusMessage::createMethodCall("org.qindaqt.Keyring1","/org/freedesktop/secrets","org.qindaqt.Keyring1","AttachSessionWithDisplay");
        message.setArguments({QStringLiteral("qindaqt-7")});
        QDBusReply<bool> reply=QDBusConnection::sessionBus().call(message);return reply.isValid() && reply.value();
    }
    void selectLogin() {
        int index=-1;const auto rows=model->collections();
        for(int i=0;i<rows.size();++i) if(rows.at(i).toMap().value("path").toString().endsWith("/login")) index=i;
        QVERIFY(index>=0);auto *combo=findVisual(window->contentItem(),"keyringCollections");QVERIFY(combo);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,combo->mapToScene(QPointF(combo->width()/2,combo->height()/2)).toPoint());
        QTest::keyClick(window,Qt::Key_Home);for(int i=0;i<index;++i) QTest::keyClick(window,Qt::Key_Down);
        QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(model->selectedCollectionPath().endsWith("/login"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),5000);
    }
private Q_SLOTS:
    void initTestCase() {
        QTRY_VERIFY_WITH_TIMEOUT(QDBusConnection::sessionBus().interface()->isServiceRegistered("org.qindaqt.Keyring1").value(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(attach(),5000);
        engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
        QQmlComponent registration(&engine);
        registration.setData("import QtQuick; import QindaQt.Tokens; QtObject {property int revision: Tokens.qstRevision}",QUrl("inline:journey-tokens"));
        QTRY_VERIFY_WITH_TIMEOUT(registration.isReady(),5000);
        std::unique_ptr<QObject> registered(registration.create());QVERIFY(registered);
        auto *tokens=engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>("QindaQt.Tokens","Tokens");QVERIFY(tokens);
        const auto theme=QindaQt::Themes::ThemeLoader::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
        QVERIFY(theme.ok);QVERIFY(tokens->publish(theme.theme,{}));
        engine.loadData(R"(
import QtQuick
import QindaQt.SettingsApp.Keyring
Window {
    id: journeyWindow
    width: 1050; height: 710; visible: true
    property bool routeActive: true
    property var journeyModel: KeyringRouteComposition.model
    Loader { anchors.fill: parent; active: journeyWindow.routeActive
        sourceComponent: KeyringPage { keyringSettings: KeyringRouteComposition.model }
    }
})");
        QVERIFY(!engine.rootObjects().isEmpty());window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());QVERIFY(window);
        model=qobject_cast<KeyringSettingsModel *>(window->property("journeyModel").value<QObject *>());QVERIFY(model);
        QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(model->available() && !model->busy() && !model->collections().isEmpty(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(model->secretsAllowed(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(model->preferences()->property("available").toBool(),5000);
        QVERIFY(!model->preferences()->property("lockOnScreenLock").toBool());
    }
    void createUnlockRevealCopyRekey() {
        auto *label=findVisual(window->contentItem(),"keyringNewCollectionLabel");QVERIFY(label);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,label->mapToScene(QPointF(30,label->height()/2)).toPoint());
        for(char c:QByteArray("journey collection")) QTest::keyClick(window,c);
        QVERIFY(click({},"keyringCreateCollection"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->collections().size()>=3,15000);
        selectLogin();QVERIFY(click("Unlock"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->items().size()==2,15000);
        QVERIFY(click("Reveal"));QTRY_VERIFY_WITH_TIMEOUT(model->secretVisible() && !model->busy(),15000);
        QVERIFY(model->secretText()==QStringLiteral("fixture-secret-only"));
        QVERIFY(click("Copy"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),15000);QVERIFY(clipboard("secret"));
        QVERIFY(click("Change password"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),15000);
        QFile phase(qEnvironmentVariable("QINDAQT_JOURNEY_PHASE"));QVERIFY(phase.open(QIODevice::WriteOnly));phase.write("1");phase.close();
        qindaqt::keyring::CollectionStore sealed(qEnvironmentVariable("QINDAQT_JOURNEY_STORAGE").toStdString(),"login");
        QVERIFY(sealed.load()==qindaqt::keyring::StoreError::None);
        QVERIFY(sealed.unlock(fixture::password())==qindaqt::keyring::StoreError::AuthenticationFailed);
        QVERIFY(sealed.unlock(fixture::otherPassword())==qindaqt::keyring::StoreError::None);sealed.lock();
        QVERIFY(click("Lock"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),5000);
        QVERIFY(click("Unlock"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->items().size()==2,15000);
        QVERIFY(click("Reveal"));QTRY_VERIFY_WITH_TIMEOUT(model->secretVisible() && !model->busy(),15000);
    }
    void pageDepartureRetiresClipboard() {
        QVERIFY(click("Copy"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),15000);QVERIFY(clipboard("secret"));
        window->setProperty("routeActive",false);
        QTRY_VERIFY(!model->secretVisible());
        QVERIFY(clipboard("empty"));QVERIFY(clipboardBytesEmpty());
        window->setProperty("routeActive",true);QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->secretsAllowed(),5000);
    }
    void ownerDepartureRetiresClipboard() {
        if(!window->property("routeActive").toBool()) window->setProperty("routeActive",true);
        QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->available(),5000);selectLogin();
        QVERIFY(click("Reveal"));QTRY_VERIFY_WITH_TIMEOUT(model->secretVisible() && !model->busy(),15000);
        QVERIFY(click("Copy"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),15000);QVERIFY(clipboard("secret"));
        const int pid=qEnvironmentVariableIntValue("QINDAQT_JOURNEY_DAEMON_PID");QVERIFY(pid>1);QVERIFY(::kill(pid,SIGTERM)==0);
        QTRY_VERIFY_WITH_TIMEOUT(!model->available() && !model->secretVisible(),5000);
        QVERIFY(clipboard("empty"));QVERIFY(clipboardBytesEmpty());
        replacement.start(qEnvironmentVariable("QINDAQT_JOURNEY_DAEMON"),{
            "--private-bus",qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),"--storage-root",qEnvironmentVariable("QINDAQT_JOURNEY_STORAGE"),
            "--runtime-root",qEnvironmentVariable("XDG_RUNTIME_DIR"),"--require-session-display","--policy-fixture","--prompt-program",qEnvironmentVariable("QINDAQT_JOURNEY_PROMPT")});
        QTRY_VERIFY_WITH_TIMEOUT(QDBusConnection::sessionBus().interface()->isServiceRegistered("org.qindaqt.Keyring1").value(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(attach(),5000);QTRY_VERIFY_WITH_TIMEOUT(model->available() && !model->busy(),5000);
        QVERIFY(click({},"keyringReload"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->available(),5000);
        selectLogin();QVERIFY(click("Unlock"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->items().size()==2,15000);
    }
    void deleteWithActualConfirmation() {
        QVERIFY(click("Delete"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy() && model->items().size()==1,15000);
        qindaqt::keyring::CollectionStore sealed(qEnvironmentVariable("QINDAQT_JOURNEY_STORAGE").toStdString(),"login");
        QVERIFY(sealed.load()==qindaqt::keyring::StoreError::None);
        QVERIFY(sealed.unlock(fixture::otherPassword())==qindaqt::keyring::StoreError::None);
        QVERIFY(sealed.search({}).ids.size()==1);sealed.lock();
    }
    void nativeLockRetiresClipboardWithoutCollectionPreference() {
        QVERIFY(click("Reveal"));QTRY_VERIFY_WITH_TIMEOUT(model->secretVisible() && !model->busy(),15000);
        QVERIFY(click("Copy"));QTRY_VERIFY_WITH_TIMEOUT(!model->busy(),15000);QVERIFY(clipboard("secret"));
        QDBusInterface lock(QString(QindaQt::CompositorNames::service),QString(QindaQt::CompositorNames::nativeLockPath),
            QString(QindaQt::CompositorNames::nativeLockInterface),QDBusConnection::sessionBus());
        lock.asyncCall("RequestLockWithReceipt",QUuid::createUuid().toString(QUuid::Id128));
        QTRY_VERIFY_WITH_TIMEOUT(!model->secretVisible() && !model->secretsAllowed(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(model->policyStatus()==QStringLiteral("Unlock the screen to reveal or copy secrets."),5000);
        QVERIFY(clipboard("empty"));QVERIFY(clipboardBytesEmpty());
    }
    void cleanupTestCase() {
        window=nullptr;model=nullptr;
        if(replacement.state()!=QProcess::NotRunning) {replacement.terminate();replacement.waitForFinished(5000);}
    }
};
int main(int argc,char **argv) {
    struct rlimit core{0,0};
    if(setrlimit(RLIMIT_CORE,&core)!=0 || prctl(PR_SET_DUMPABLE,0)!=0) return 2;
    if(argc==2 && QByteArray(argv[1])=="--seed") {
        QCoreApplication app(argc,argv);qindaqt::keyring::CollectionStore store(qEnvironmentVariable("QINDAQT_JOURNEY_STORAGE").toStdString(),"login");
        using qindaqt::keyring::StoreError;
        return store.create(fixture::password(),fixture::fastKdf())==StoreError::None
            && store.put(fixture::item())==StoreError::None && store.put(fixture::item("item-two"))==StoreError::None
            && store.save()==StoreError::None?0:2;
    }
    QGuiApplication app(argc,argv);NativeUiJourney test;return QTest::qExec(&test,argc,argv);
}
#include "tst_native_ui.moc"
