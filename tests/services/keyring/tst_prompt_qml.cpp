// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/services/keyring/prompt/prompt_controller.h"
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <unistd.h>
#include <fcntl.h>
using qindaqt::keyring::service::PromptController;
class KeyringPromptTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void passwordChangeRequiresOldAndMatchingNewAndWritesOnlyBoundedFrame() {
        PromptController prompt; prompt.changing = true;
        QVERIFY(!prompt.approve("synthetic-new","different","synthetic-old"));
        QVERIFY(!prompt.approve("synthetic-new","synthetic-new",""));
        int descriptors[2];QVERIFY(pipe2(descriptors,O_CLOEXEC)==0);
        const int previous=dup(STDOUT_FILENO);QVERIFY(previous>=0);
        QVERIFY(dup2(descriptors[1],STDOUT_FILENO)>=0);close(descriptors[1]);
        const bool approved=prompt.approve("synthetic-new","synthetic-new","synthetic-old");
        const int restored=dup2(previous,STDOUT_FILENO);close(previous);
        QByteArray frame(128,Qt::Uninitialized);
        const auto count=read(descriptors[0],frame.data(),128);close(descriptors[0]);
        QVERIFY(restored>=0);QVERIFY(approved);QVERIFY(count>0);frame.resize(count);
        QCOMPARE(frame,QByteArray("QKP1")+QByteArray::fromHex("000d000d")+"synthetic-oldsynthetic-new");
    }
    void deletionConfirmationWritesNoPasswordPayload() {
        PromptController prompt;prompt.confirming=true;
        int descriptors[2];QVERIFY(pipe2(descriptors,O_CLOEXEC)==0);
        const int previous=dup(STDOUT_FILENO);QVERIFY(previous>=0);
        QVERIFY(dup2(descriptors[1],STDOUT_FILENO)>=0);close(descriptors[1]);
        const bool approved=prompt.approve("","","");
        const int restored=dup2(previous,STDOUT_FILENO);close(previous);
        QByteArray frame(32,Qt::Uninitialized);
        const auto count=read(descriptors[0],frame.data(),32);close(descriptors[0]);
        QVERIFY(restored>=0);QVERIFY(approved);QVERIFY(count>0);frame.resize(count);
        QCOMPARE(frame,QByteArray("QKOK"));
    }

    void hiddenSurfaceAndPasswordFields() {
        PromptController prompt; prompt.creation = true; prompt.label = "Synthetic collection";
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("prompt",&prompt);
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(QINDAQT_KEYRING_PROMPT_QML)));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr,qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window); QVERIFY(!window->isVisible());
        auto *password = root->findChild<QObject *>("keyringPasswordField");
        auto *confirmation = root->findChild<QObject *>("keyringConfirmationField");
        QVERIFY(password); QVERIFY(confirmation);
        QCOMPARE(password->property("echoMode").toInt(),2); // TextInput.Password
        QCOMPARE(confirmation->property("echoMode").toInt(),2);
        QCOMPARE(password->property("maximumLength").toInt(),4096);
        QString error;
        QVERIFY(!QindaQt::AuthenticationOverlay::OverlaySurface::configure(*window,"keyring-test",&error));
        QVERIFY(!error.isEmpty()); QVERIFY(!window->isVisible());
    }
    void mismatchAndEmptyPasswordsClearWithoutApproval() {
        PromptController prompt; prompt.creation = true;
        QSignalSpy cleared(&prompt,&PromptController::clearFields);
        QVERIFY(!prompt.approve("synthetic","different"));
        QVERIFY(!prompt.approve("",""));
        QCOMPARE(cleared.size(),2);
    }
};
QTEST_MAIN(KeyringPromptTest)
#include "tst_prompt_qml.moc"
