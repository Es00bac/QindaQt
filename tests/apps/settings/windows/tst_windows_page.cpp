// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_windows_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsWindows::TestSupport::StubWindowsSettingsModel;

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
} // namespace

class WindowsPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void rendersWideControlsWithAdmittedFocus();
    void rendersCompactAndFollowsTheDraft();
    void wiresDraftApplyAndRevertActions();
    void presentsUnavailableWithRetry();

private:
    std::unique_ptr<QQuickView> m_view;
    std::unique_ptr<StubWindowsSettingsModel> m_model;
    std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void WindowsPageTest::initTestCase()
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

std::pair<std::unique_ptr<QObject>, QQuickItem *> WindowsPageTest::createPage(QSize size)
{
    m_model = std::make_unique<StubWindowsSettingsModel>();
    QQmlComponent component(m_view->engine());
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(QINDAQT_WINDOWS_PAGE_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("windowsSettings"),
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

void WindowsPageTest::rendersWideControlsWithAdmittedFocus()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *focusPolicy = findItem(page, QStringLiteral("windowsFocusPolicySelector"));
    auto *docking = findItem(page, QStringLiteral("windowsDockingModifierSelector"));
    auto *closePolicy = findItem(page, QStringLiteral("windowsClosePolicySelector"));
    auto *slider = findItem(page, QStringLiteral("windowsSnapDistanceSlider"));
    auto *row = findItem(page, QStringLiteral("windowsSnapDistanceRow"));
    auto *focusRow = findItem(page, QStringLiteral("windowsFocusPolicyRow"));
    auto *apply = findItem(page, QStringLiteral("windowsApplyButton"));
    auto *revert = findItem(page, QStringLiteral("windowsRevertButton"));
    auto *retry = findItem(page, QStringLiteral("windowsRetryButton"));
    auto *notice = findItem(page, QStringLiteral("windowsDegradedNotice"));
    QVERIFY(focusPolicy != nullptr);
    QVERIFY(docking != nullptr);
    QVERIFY(closePolicy != nullptr);
    QVERIFY(slider != nullptr);
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("columns").toInt(), 3);
    QVERIFY(focusRow != nullptr);
    QCOMPARE(focusRow->property("columns").toInt(), 2);
    QVERIFY(apply != nullptr);
    QVERIFY(revert != nullptr);
    QVERIFY(retry != nullptr);
    QVERIFY(notice != nullptr);
    QVERIFY(!notice->isVisible());
    QVERIFY(!retry->isVisible());
    // Nothing is dirty yet, so neither Apply nor Revert is admitted.
    QVERIFY(!apply->isEnabled());
    QVERIFY(!revert->isEnabled());
    QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), focusPolicy);
    focusPolicy->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), focusPolicy);

    // The selectors show the draft token's label, never the token.
    QCOMPARE(focusPolicy->property("currentIndex").toInt(), 0);
    QCOMPARE(focusPolicy->property("currentText").toString(), QStringLiteral("Click to focus"));
    QCOMPARE(docking->property("currentText").toString(), QStringLiteral("Meta + Shift"));
    QCOMPARE(closePolicy->property("currentText").toString(), QStringLiteral("Ask every time"));
    const auto *comboAccessible = QAccessible::queryAccessibleInterface(focusPolicy);
    QVERIFY(comboAccessible != nullptr);
    QCOMPARE(comboAccessible->role(), QAccessible::ComboBox);
    QVERIFY(!comboAccessible->text(QAccessible::Description).isEmpty());
    QCOMPARE(focusPolicy->property("accessibleDescription").toString(),
             comboAccessible->text(QAccessible::Description));
    const auto *sliderAccessible = QAccessible::queryAccessibleInterface(slider);
    QVERIFY(sliderAccessible != nullptr);
    QCOMPARE(sliderAccessible->role(), QAccessible::Slider);
    QCOMPARE(sliderAccessible->text(QAccessible::Name), QStringLiteral("Snap distance"));
    QVERIFY(sliderAccessible->text(QAccessible::Description).contains(QStringLiteral("default")));
    QCOMPARE(slider->property("from").toDouble(), 0.0);
    QCOMPARE(slider->property("to").toDouble(), 64.0);
    QCOMPARE(slider->property("value").toDouble(), 12.0);
}

void WindowsPageTest::rendersCompactAndFollowsTheDraft()
{
    auto [guard, page] = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *row = findItem(page, QStringLiteral("windowsSnapDistanceRow"));
    auto *focusRow = findItem(page, QStringLiteral("windowsFocusPolicyRow"));
    auto *slider = findItem(page, QStringLiteral("windowsSnapDistanceSlider"));
    auto *value = findItem(page, QStringLiteral("windowsSnapDistanceValue"));
    auto *docking = findItem(page, QStringLiteral("windowsDockingModifierSelector"));
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("columns").toInt(), 1);
    QVERIFY(focusRow != nullptr);
    QCOMPARE(focusRow->property("columns").toInt(), 1);
    QVERIFY(slider != nullptr);
    QVERIFY(value != nullptr);
    QVERIFY(docking != nullptr);
    QCOMPARE(value->property("text").toString(), QStringLiteral("12 px"));

    // A model-side draft change (Revert, remote change) moves the slider and
    // the selectors without any activation being reported back.
    m_model->draftSnapDistance = 40;
    m_model->draftDockingModifier = QStringLiteral("disabled");
    Q_EMIT m_model->viewChanged();
    QTRY_COMPARE(slider->property("value").toDouble(), 40.0);
    QCOMPARE(value->property("text").toString(), QStringLiteral("40 px"));
    QTRY_COMPARE(docking->property("currentIndex").toInt(), 3);
    QCOMPARE(docking->property("currentText").toString(), QStringLiteral("Off"));
    QCOMPARE(m_model->dockingModifierRequests, 0);
    QCOMPARE(m_model->snapDistanceRequests, 0);
}

void WindowsPageTest::wiresDraftApplyAndRevertActions()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *focusPolicy = findItem(page, QStringLiteral("windowsFocusPolicySelector"));
    auto *docking = findItem(page, QStringLiteral("windowsDockingModifierSelector"));
    auto *closePolicy = findItem(page, QStringLiteral("windowsClosePolicySelector"));
    auto *slider = findItem(page, QStringLiteral("windowsSnapDistanceSlider"));
    auto *apply = findItem(page, QStringLiteral("windowsApplyButton"));
    auto *revert = findItem(page, QStringLiteral("windowsRevertButton"));
    QVERIFY(focusPolicy != nullptr);
    QVERIFY(docking != nullptr);
    QVERIFY(closePolicy != nullptr);
    QVERIFY(slider != nullptr);
    QVERIFY(apply != nullptr);
    QVERIFY(revert != nullptr);

    // Only an interactive activation writes the draft, and it writes the
    // schema token behind the chosen label.
    QVERIFY(QMetaObject::invokeMethod(focusPolicy, "activated", Q_ARG(int, 1)));
    QCOMPARE(m_model->focusPolicyRequests, 1);
    QCOMPARE(m_model->lastFocusPolicy, QStringLiteral("focus-follows-mouse"));
    QVERIFY(QMetaObject::invokeMethod(docking, "activated", Q_ARG(int, 2)));
    QCOMPARE(m_model->dockingModifierRequests, 1);
    QCOMPARE(m_model->lastDockingModifier, QStringLiteral("control"));
    QVERIFY(QMetaObject::invokeMethod(closePolicy, "activated", Q_ARG(int, 2)));
    QCOMPARE(m_model->closePolicyRequests, 1);
    QCOMPARE(m_model->lastClosePolicy, QStringLiteral("ungroup"));

    // Only an interactive move writes the distance, rounded to whole pixels.
    slider->setProperty("value", 30.0);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QCOMPARE(m_model->snapDistanceRequests, 1);
    QCOMPARE(m_model->lastSnapDistance, 30);

    QTRY_VERIFY(apply->isEnabled());
    QVERIFY(revert->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
    QCOMPARE(m_model->applyRequests, 1);
    QVERIFY(QMetaObject::invokeMethod(revert, "clicked"));
    QCOMPARE(m_model->revertRequests, 1);

    // Saving fences every editor and shows the busy Apply.
    m_model->saving = true;
    m_model->ready = false;
    m_model->canEdit = false;
    m_model->applyAvailable = false;
    Q_EMIT m_model->viewChanged();
    QTRY_VERIFY(!focusPolicy->isEnabled());
    QVERIFY(!docking->isEnabled());
    QVERIFY(!closePolicy->isEnabled());
    QVERIFY(!slider->isEnabled());
    QVERIFY(!apply->isEnabled());
    QVERIFY(!revert->isEnabled());
    QCOMPARE(apply->property("busy").toBool(), true);
}

void WindowsPageTest::presentsUnavailableWithRetry()
{
    auto [guard, page] = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    m_model->ready = false;
    m_model->unavailable = true;
    m_model->canEdit = false;
    m_model->applyAvailable = false;
    m_model->statusText = QStringLiteral(
        "Settings1 is unavailable; the last confirmed values are shown.");
    Q_EMIT m_model->viewChanged();
    QCoreApplication::processEvents();
    auto *notice = findItem(page, QStringLiteral("windowsDegradedNotice"));
    auto *focusPolicy = findItem(page, QStringLiteral("windowsFocusPolicySelector"));
    auto *slider = findItem(page, QStringLiteral("windowsSnapDistanceSlider"));
    auto *retry = findItem(page, QStringLiteral("windowsRetryButton"));
    auto *status = findItem(page, QStringLiteral("windowsStatus"));
    QVERIFY(notice != nullptr);
    QTRY_VERIFY(notice->isVisible());
    const auto *noticeAccessible = QAccessible::queryAccessibleInterface(notice);
    QVERIFY(noticeAccessible != nullptr);
    QCOMPARE(noticeAccessible->role(), QAccessible::AlertMessage);
    QVERIFY(noticeAccessible->text(QAccessible::Description)
                .contains(QStringLiteral("unavailable")));
    QVERIFY(focusPolicy != nullptr);
    QVERIFY(!focusPolicy->isEnabled());
    QVERIFY(slider != nullptr);
    QVERIFY(!slider->isEnabled());
    // The unavailable reason lives in the notice; it is not repeated below.
    QVERIFY(status != nullptr);
    QVERIFY(!status->isVisible());
    QVERIFY(retry != nullptr);
    QVERIFY(retry->isVisible());
    QVERIFY(retry->isEnabled());
    QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), retry);
    QVERIFY(QMetaObject::invokeMethod(retry, "clicked"));
    QCOMPARE(m_model->retryRequests, 1);
    QVERIFY(QMetaObject::invokeMethod(notice, "retryRequested"));
    QCOMPARE(m_model->retryRequests, 2);
}

QTEST_MAIN(WindowsPageTest)
#include "tst_windows_page.moc"
