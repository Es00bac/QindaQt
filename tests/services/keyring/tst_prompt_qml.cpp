// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/services/keyring/prompt/prompt_controller.h"
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
using qindaqt::keyring::service::PromptController;
class KeyringPromptTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
