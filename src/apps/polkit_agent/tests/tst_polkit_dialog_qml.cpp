// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_agent_appearance.h"
#include "polkit_attempt_controller.h"
#include "polkit_dialog_view_model.h"
#include "polkit_request_queue.h"

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>
#include <utility>

using namespace QindaQt::Apps::PolkitAgent;

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

class FakeAttempt final : public AuthenticationAttempt {
public:
    QStringList responses;
    int cancelCount = 0;
    void respond(const QString &response) override { responses.append(response); }
    void cancel() override { ++cancelCount; }
};

QList<AgentIdentity> oneIdentity()
{
    return {{QStringLiteral("Jarrod C (jarrod)"), QStringLiteral("unix-user:1000"), true}};
}

AuthenticationRequest makeRequest(QList<AgentIdentity> identities)
{
    AuthenticationRequest request;
    request.actionId = QStringLiteral("org.qindaqt.example.test");
    request.message = QStringLiteral("Type your password to continue.");
    request.cookie = QStringLiteral("cookie-1");
    request.identities = std::move(identities);
    request.requester.displayName = QStringLiteral("Example Tool");
    return request;
}

} // namespace

class PolkitDialogQmlTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void hidesIdentityChooserForOneIdentityAndExposesAccessibleNames();
    void enterAuthenticatesAndEscapeCancels();
    void failedAttemptShowsErrorClearsAndRefocusesPassword();

private:
    std::unique_ptr<QQuickView> m_view;
    std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(PolkitDialogViewModel &viewModel);
};

void PolkitDialogQmlTest::initTestCase()
{
    m_view = std::make_unique<QQuickView>();
    m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = ensurePolkitAgentTokenFacade(*m_view->engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
PolkitDialogQmlTest::createPage(PolkitDialogViewModel &viewModel)
{
    QQmlComponent component(m_view->engine());
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(QINDAQT_POLKIT_DIALOG_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    QObject *object = component.createWithInitialProperties(
        {{QStringLiteral("viewModel"), QVariant::fromValue(static_cast<QObject *>(&viewModel))}});
    if (object == nullptr) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    auto guard = std::unique_ptr<QObject>(object);
    auto *page = qobject_cast<QQuickItem *>(object);
    if (page == nullptr) {
        return {};
    }
    m_view->resize(QSize(480, 640));
    page->setParentItem(m_view->contentItem());
    page->setSize(QSize(480, 640));
    m_view->show();
    QCoreApplication::processEvents();
    return {std::move(guard), page};
}

void PolkitDialogQmlTest::hidesIdentityChooserForOneIdentityAndExposesAccessibleNames()
{
    PolkitAttemptController controller(oneIdentity(), 0, [](const QString &) {
        return std::make_unique<FakeAttempt>();
    });
    controller.start();
    PolkitDialogViewModel viewModel;
    viewModel.attachRequest(makeRequest(oneIdentity()), &controller);
    auto [guard, page] = createPage(viewModel);
    QVERIFY(page != nullptr);

    auto *chooser = findItem(page, QStringLiteral("polkitAgentIdentityChooser"));
    QVERIFY(chooser != nullptr);
    QVERIFY(!chooser->isVisible());

    auto *passwordField = findItem(page, QStringLiteral("polkitAgentResponseField"));
    auto *cancelButton = findItem(page, QStringLiteral("polkitAgentCancelButton"));
    auto *authenticateButton = findItem(page, QStringLiteral("polkitAgentAuthenticateButton"));
    QVERIFY(passwordField != nullptr);
    QVERIFY(cancelButton != nullptr);
    QVERIFY(authenticateButton != nullptr);

    auto *passwordAccessible = QAccessible::queryAccessibleInterface(passwordField);
    QVERIFY(passwordAccessible != nullptr);
    QVERIFY(!passwordAccessible->text(QAccessible::Name).isEmpty());
    auto *cancelAccessible = QAccessible::queryAccessibleInterface(cancelButton);
    QVERIFY(cancelAccessible != nullptr);
    QCOMPARE(cancelAccessible->text(QAccessible::Name), QStringLiteral("Cancel"));
    auto *authenticateAccessible = QAccessible::queryAccessibleInterface(authenticateButton);
    QVERIFY(authenticateAccessible != nullptr);
    QCOMPARE(authenticateAccessible->text(QAccessible::Name), QStringLiteral("Authenticate"));
}

void PolkitDialogQmlTest::enterAuthenticatesAndEscapeCancels()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    controller.start();
    PolkitDialogViewModel viewModel;
    viewModel.attachRequest(makeRequest(oneIdentity()), &controller);
    // The dialog disables the response field until PAM actually asks for
    // something (viewModel.busy), exactly as the real Session's first
    // request() signal would arrive; simulate that here.
    created.constFirst()->request(QStringLiteral("Password:"), false);
    auto [guard, page] = createPage(viewModel);
    QVERIFY(page != nullptr);
    auto *passwordField = findItem(page, QStringLiteral("polkitAgentResponseField"));
    QVERIFY(passwordField != nullptr);

    passwordField->forceActiveFocus();
    QTRY_COMPARE(m_view->activeFocusItem(), passwordField);
    for (const QChar &character : QStringLiteral("hunter2")) {
        QTest::keyClick(m_view.get(), character.toLatin1());
    }
    QTest::keyClick(m_view.get(), Qt::Key_Return);
    QCOMPARE(created.constFirst()->responses, QStringList{QStringLiteral("hunter2")});

    QTest::keyClick(m_view.get(), Qt::Key_Escape);
    QCOMPARE(created.constFirst()->cancelCount, 1);
}

void PolkitDialogQmlTest::failedAttemptShowsErrorClearsAndRefocusesPassword()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    controller.start();
    PolkitDialogViewModel viewModel;
    viewModel.attachRequest(makeRequest(oneIdentity()), &controller);
    created.constFirst()->request(QStringLiteral("Password:"), false);
    auto [guard, page] = createPage(viewModel);
    QVERIFY(page != nullptr);
    auto *passwordField = findItem(page, QStringLiteral("polkitAgentResponseField"));
    auto *statusLine = findItem(page, QStringLiteral("polkitAgentStatusLine"));
    QVERIFY(passwordField != nullptr);
    QVERIFY(statusLine != nullptr);

    passwordField->setProperty("text", QStringLiteral("wrong-password"));
    created.constFirst()->completed(false);
    QCoreApplication::processEvents();

    QVERIFY(statusLine->isVisible());
    QCOMPARE(statusLine->property("text").toString(),
             QStringLiteral("That password didn't work. Try again."));
    QCOMPARE(passwordField->property("text").toString(), QString());
    QTRY_COMPARE(m_view->activeFocusItem(), passwordField);
}

QTEST_MAIN(PolkitDialogQmlTest)
#include "tst_polkit_dialog_qml.moc"
