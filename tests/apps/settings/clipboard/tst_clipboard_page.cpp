// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_clipboard_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsClipboard::TestSupport::StubClipboardSettingsModel;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root == nullptr) return nullptr;
    if (root->objectName() == name) return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *found = findItem(child, name)) return found;
    }
    return nullptr;
}
}

class ClipboardPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void rendersWidePrivacyAndCountTruth();
    void rendersCompactWithAdmittedFocus();
    void wiresPreferenceAndConfirmationActions();
    void presentsDegradedPrivacyAndUncertainTruth();

private:
    std::unique_ptr<QQuickView> m_view;
    std::unique_ptr<StubClipboardSettingsModel> m_model;
    std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void ClipboardPageTest::initTestCase()
{
    m_view = std::make_unique<QQuickView>();
    m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *m_view->engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
ClipboardPageTest::createPage(QSize size)
{
    m_model = std::make_unique<StubClipboardSettingsModel>();
    QQmlComponent component(m_view->engine());
    component.loadUrl(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_CLIPBOARD_PAGE_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("clipboardSettings"),
         QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
    });
    if (object == nullptr) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    auto guard = std::unique_ptr<QObject>(object);
    auto *page = qobject_cast<QQuickItem *>(object);
    if (page == nullptr) return {};
    m_view->resize(size);
    page->setParentItem(m_view->contentItem());
    page->setSize(size);
    m_view->show();
    QCoreApplication::processEvents();
    return {std::move(guard), page};
}

void ClipboardPageTest::rendersWidePrivacyAndCountTruth()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *privacy = findItem(page, QStringLiteral("clipboardPrivacyNotice"));
    auto *boundary = findItem(page, QStringLiteral("clipboardContentBoundary"));
    auto *grid = findItem(page, QStringLiteral("clipboardStateGrid"));
    auto *clear = findItem(page, QStringLiteral("clipboardClearButton"));
    QVERIFY(privacy != nullptr);
    QVERIFY(boundary != nullptr);
    QVERIFY(grid != nullptr);
    QCOMPARE(grid->property("columns").toInt(), 2);
    QVERIFY(clear != nullptr);
    QVERIFY(clear->isEnabled());
    const auto *boundaryAccessible = QAccessible::queryAccessibleInterface(boundary);
    QVERIFY(boundaryAccessible != nullptr);
    QVERIFY(boundaryAccessible->text(QAccessible::Description)
                .contains(QStringLiteral("cannot read")));
}

void ClipboardPageTest::rendersCompactWithAdmittedFocus()
{
    auto [guard, page] = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *grid = findItem(page, QStringLiteral("clipboardStateGrid"));
    auto *close = findItem(page, QStringLiteral("clipboardCloseButton"));
    QVERIFY(grid != nullptr);
    QCOMPARE(grid->property("columns").toInt(), 1);
    QVERIFY(close != nullptr);
    QVERIFY(close->isEnabled());
    QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), close);
    close->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), close);
    const auto *accessible = QAccessible::queryAccessibleInterface(close);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::Button);
}

void ClipboardPageTest::wiresPreferenceAndConfirmationActions()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *history = findItem(page, QStringLiteral("clipboardHistorySwitch"));
    auto *clear = findItem(page, QStringLiteral("clipboardClearButton"));
    QVERIFY(history != nullptr);
    QVERIFY(clear != nullptr);
    history->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(history, "toggled"));
    QCOMPARE(m_model->draftRequests, 1);
    QCOMPARE(m_model->applyRequests, 1);
    QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
    QCOMPARE(m_model->clearRequests, 1);
    QObject *dialog = page->findChild<QObject *>(
        QStringLiteral("clipboardClearDialog"), Qt::FindChildrenRecursively);
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog, "rejected"));
    QCOMPARE(m_model->cancelClearRequests, 1);
    QTRY_VERIFY(!dialog->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog, "accepted"));
    QCOMPARE(m_model->confirmRequests, 1);
}

void ClipboardPageTest::presentsDegradedPrivacyAndUncertainTruth()
{
    auto [guard, page] = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    m_model->serviceAvailable = false;
    m_model->serviceDegraded = true;
    m_model->privacyDenied = true;
    m_model->clearAvailable = false;
    m_model->clearUncertain = true;
    m_model->clearStatusText = QStringLiteral(
        "The clear outcome is uncertain. It was not retried.");
    Q_EMIT m_model->viewChanged();
    QCoreApplication::processEvents();
    auto *service = findItem(page, QStringLiteral("clipboardServiceState"));
    auto *status = findItem(page, QStringLiteral("clipboardClearStatus"));
    auto *clear = findItem(page, QStringLiteral("clipboardClearButton"));
    QVERIFY(service != nullptr);
    QCOMPARE(service->property("status").toInt(), 2); // StateCard.Warning
    QVERIFY(status != nullptr);
    const auto *accessible = QAccessible::queryAccessibleInterface(status);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::AlertMessage);
    QVERIFY(clear != nullptr);
    QVERIFY(!clear->isEnabled());
}

QTEST_MAIN(ClipboardPageTest)
#include "tst_clipboard_page.moc"
