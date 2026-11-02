// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/services/keyring/prompt/prompt_controller.h"
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>
#include <unistd.h>
#include <fcntl.h>
using qindaqt::keyring::service::PromptController;
class KeyringPromptTest final : public QObject {
    Q_OBJECT
    static qindaqt::keyring::protocol::PromptMetadata metadata(const QString &action = "unlock") {
        qindaqt::keyring::protocol::PromptMetadata value;
        value.label = "Synthetic collection"; value.caller = ":1.42";
        value.executable = "/usr/bin/python3"; value.processId = 12345; value.userId = 1000;
        value.parents = {"/usr/bin/agent (PID 123)"}; value.operation = action;
        return value;
    }
private Q_SLOTS:
    void passwordChangeRequiresOldAndMatchingNewAndWritesOnlyBoundedFrame() {
        PromptController prompt;
        QVERIFY(prompt.setRequest("change-password", metadata("change-password"))); QVERIFY(prompt.review());
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
        PromptController prompt;
        QVERIFY(prompt.setRequest("confirm-delete", metadata("confirm-delete"))); QVERIFY(prompt.review());
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
        PromptController prompt;
        QVERIFY(prompt.setRequest("create", metadata("create")));
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
        QVERIFY(!password->property("visible").toBool());
        QVERIFY(!password->property("enabled").toBool());
        QString error;
        QVERIFY(!QindaQt::AuthenticationOverlay::OverlaySurface::configure(*window,"keyring-test",&error));
        QVERIFY(!error.isEmpty()); QVERIFY(!window->isVisible());
    }
    void mismatchAndEmptyPasswordsClearWithoutApproval() {
        PromptController prompt;
        QVERIFY(prompt.setRequest("create", metadata("create"))); QVERIFY(prompt.review());
        QSignalSpy cleared(&prompt,&PromptController::clearFields);
        QVERIFY(!prompt.approve("synthetic","different"));
        QVERIFY(!prompt.approve("",""));
        QCOMPARE(cleared.size(),2);
    }
    void activatedPromptRestoresPasswordFocus() {
        PromptController prompt;
        QVERIFY(prompt.setRequest("unlock", metadata()));
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("prompt",&prompt);
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(QINDAQT_KEYRING_PROMPT_QML)));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr,qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        auto *password = root->findChild<QQuickItem *>("keyringPasswordField");
        auto *button = root->findChild<QQuickItem *>("keyringApproveButton");
        auto *deny = root->findChild<QQuickItem *>("keyringDenyButton");
        auto *review = root->findChild<QQuickItem *>("keyringReviewButton");
        QVERIFY(window); QVERIFY(password); QVERIFY(button); QVERIFY(deny); QVERIFY(review);
        window->resize(800, 900);
        window->show(); window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTRY_VERIFY(deny->hasActiveFocus());
        QVERIFY(!password->isVisible()); QVERIFY(!button->isVisible());
        QVERIFY(QMetaObject::invokeMethod(review, "clicked"));
        QVERIFY(prompt.reviewed());
        QTRY_VERIFY(password->hasActiveFocus());
        password->setProperty("text",QStringLiteral("synthetic"));
        button->forceActiveFocus();
        QTRY_VERIFY(button->hasActiveFocus());
        QQuickWindow other;
        other.show(); other.requestActivate();
        QTRY_VERIFY(other.isActive());
        QTRY_VERIFY(!window->isActive());
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTRY_VERIFY(password->hasActiveFocus());
    }
    void unknownIdentityAndUnreviewedRequestsCannotApprove() {
        PromptController prompt;
        QVERIFY(prompt.setRequest("unlock", metadata()));
        QVERIFY(!prompt.approve("synthetic", ""));
        QVERIFY(!prompt.reviewed());
        PromptController unknown;
        auto missing = metadata(); missing.processId = 0; missing.executable.clear();
        QVERIFY(unknown.setRequest("unlock", missing));
        QVERIFY(!unknown.requesterKnown); QVERIFY(!unknown.review());
        QVERIFY(!unknown.approve("synthetic", ""));
        QVERIFY(!unknown.setRequest("create", metadata()));
    }
    void protectedExecutableIsExplicitlyUnavailable() {
        PromptController prompt;
        auto request = metadata(); request.executable.clear();
        QVERIFY(prompt.setRequest("unlock", request));
        QVERIFY(prompt.requesterKnown); QVERIFY(!prompt.executableKnown);
        QVERIFY(prompt.requester.contains("unavailable")); QVERIFY(prompt.requester.contains("12345"));
        QVERIFY(!prompt.approve("synthetic", ""));
        QVERIFY(prompt.review());
    }
    void requesterScopeAndHostileTextRemainVisibleAtSmallSizes() {
        PromptController prompt;
        auto request = metadata();
        request.label = "<b>trusted label</b>\nSpoofed heading";
        request.parents = {QStringLiteral("/tmp/agent\u202eexe (PID 123)")};
        QVERIFY(prompt.setRequest("unlock", request));
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("prompt", &prompt);
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(QINDAQT_KEYRING_PROMPT_QML)));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get()); QVERIFY(window);
        for (const auto &name : {"keyringRequester", "keyringRequesterDetails", "keyringParentContext",
                                 "keyringOperation", "keyringTaskReason", "keyringAccessScope", "keyringTarget"}) {
            const auto *label = root->findChild<QObject *>(name); QVERIFY(label);
            QCOMPARE(label->property("textFormat").toInt(), 0); // Text.PlainText
            QVERIFY(!label->property("text").toString().isEmpty());
        }
        QVERIFY(prompt.label.contains("\\u000a")); QVERIFY(prompt.parentContext.contains("\\u202e"));
        QVERIFY(prompt.scope.contains("other programs")); QVERIFY(prompt.scope.contains("same password"));
        for (const auto size : {QSize(1024, 900), QSize(640, 480)}) {
            window->resize(size); window->show(); QTest::qWait(30);
            auto *deny = root->findChild<QQuickItem *>("keyringDenyButton"); QVERIFY(deny);
            const auto bounds = deny->mapRectToScene(deny->boundingRect());
            QVERIFY(bounds.top() >= 0); QVERIFY(bounds.bottom() <= size.height());
            QVERIFY(bounds.left() >= 0); QVERIFY(bounds.right() <= size.width());
        }
        const auto screenshot = qEnvironmentVariable("QINDAQT_PROMPT_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            window->resize(1024, 900); QTest::qWait(50);
            QVERIFY(window->grabWindow().save(screenshot));
        }
    }
};
QTEST_MAIN(KeyringPromptTest)
#include "tst_prompt_qml.moc"
