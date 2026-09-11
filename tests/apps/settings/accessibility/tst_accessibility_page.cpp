// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_accessibility_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QFont>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsAccessibility::TestSupport::StubAccessibilitySettingsModel;

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

qreal pointSizeOf(QQuickItem *item)
{
    return item->property("font").value<QFont>().pointSizeF();
}
} // namespace

class AccessibilityPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void rendersWideControlsWithAdmittedFocus();
    void rendersCompactAndScalesSampleFromDraft();
    void wiresDraftApplyAndRevertActions();
    void presentsUnavailableWithRetry();

private:
    std::unique_ptr<QQuickView> m_view;
    std::unique_ptr<StubAccessibilitySettingsModel> m_model;
    std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void AccessibilityPageTest::initTestCase()
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
AccessibilityPageTest::createPage(QSize size)
{
    m_model = std::make_unique<StubAccessibilitySettingsModel>();
    QQmlComponent component(m_view->engine());
    component.loadUrl(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_ACCESSIBILITY_PAGE_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("accessibilitySettings"),
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

void AccessibilityPageTest::rendersWideControlsWithAdmittedFocus()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *highContrast = findItem(page, QStringLiteral("accessibilityHighContrastSwitch"));
    auto *reducedMotion = findItem(page, QStringLiteral("accessibilityReducedMotionSwitch"));
    auto *reducedTransparency =
        findItem(page, QStringLiteral("accessibilityReducedTransparencySwitch"));
    auto *slider = findItem(page, QStringLiteral("accessibilityTextScaleSlider"));
    auto *row = findItem(page, QStringLiteral("accessibilityTextScaleRow"));
    auto *apply = findItem(page, QStringLiteral("accessibilityApplyButton"));
    auto *revert = findItem(page, QStringLiteral("accessibilityRevertButton"));
    auto *retry = findItem(page, QStringLiteral("accessibilityRetryButton"));
    auto *notice = findItem(page, QStringLiteral("accessibilityDegradedNotice"));
    QVERIFY(highContrast != nullptr);
    QVERIFY(reducedMotion != nullptr);
    QVERIFY(reducedTransparency != nullptr);
    QVERIFY(slider != nullptr);
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("columns").toInt(), 3);
    QVERIFY(apply != nullptr);
    QVERIFY(revert != nullptr);
    QVERIFY(retry != nullptr);
    QVERIFY(notice != nullptr);
    QVERIFY(!notice->isVisible());
    QVERIFY(!retry->isVisible());
    // Nothing is dirty yet, so neither Apply nor Revert is admitted.
    QVERIFY(!apply->isEnabled());
    QVERIFY(!revert->isEnabled());
    QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), highContrast);
    highContrast->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), highContrast);

    const auto *switchAccessible = QAccessible::queryAccessibleInterface(highContrast);
    QVERIFY(switchAccessible != nullptr);
    QCOMPARE(switchAccessible->role(), QAccessible::CheckBox);
    QCOMPARE(switchAccessible->text(QAccessible::Name), QStringLiteral("High contrast"));
    QVERIFY(!switchAccessible->text(QAccessible::Description).isEmpty());
    // Explanations live in the tooltip and accessible description, not prose.
    QCOMPARE(highContrast->property("accessibleDescription").toString(),
             switchAccessible->text(QAccessible::Description));
    const auto *sliderAccessible = QAccessible::queryAccessibleInterface(slider);
    QVERIFY(sliderAccessible != nullptr);
    QCOMPARE(sliderAccessible->role(), QAccessible::Slider);
    QCOMPARE(sliderAccessible->text(QAccessible::Name), QStringLiteral("Text scale"));
    QVERIFY(sliderAccessible->text(QAccessible::Description).contains(QStringLiteral("default")));
    QCOMPARE(slider->property("from").toDouble(), 0.5);
    QCOMPARE(slider->property("to").toDouble(), 3.0);
    QCOMPARE(slider->property("value").toDouble(), 1.0);
}

void AccessibilityPageTest::rendersCompactAndScalesSampleFromDraft()
{
    auto [guard, page] = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *row = findItem(page, QStringLiteral("accessibilityTextScaleRow"));
    auto *slider = findItem(page, QStringLiteral("accessibilityTextScaleSlider"));
    auto *sample = findItem(page, QStringLiteral("accessibilityTextScaleSample"));
    auto *value = findItem(page, QStringLiteral("accessibilityTextScaleValue"));
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("columns").toInt(), 1);
    QVERIFY(slider != nullptr);
    QVERIFY(sample != nullptr);
    QVERIFY(value != nullptr);
    const qreal basePointSize = pointSizeOf(sample);
    QVERIFY(basePointSize > 0);
    QCOMPARE(value->property("text").toString(), QStringLiteral("1.00×"));

    // A model-side draft change (Revert, remote change) moves the slider and
    // the live sample follows the slider.
    m_model->draftTextScale = 2.0;
    Q_EMIT m_model->viewChanged();
    QTRY_COMPARE(slider->property("value").toDouble(), 2.0);
    QTRY_COMPARE(pointSizeOf(sample), basePointSize * 2.0);
    QCOMPARE(value->property("text").toString(), QStringLiteral("2.00×"));
    const auto *sampleAccessible = QAccessible::queryAccessibleInterface(sample);
    QVERIFY(sampleAccessible != nullptr);
    QVERIFY(sampleAccessible->text(QAccessible::Name).contains(QStringLiteral("2.00×")));
}

void AccessibilityPageTest::wiresDraftApplyAndRevertActions()
{
    auto [guard, page] = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *highContrast = findItem(page, QStringLiteral("accessibilityHighContrastSwitch"));
    auto *reducedMotion = findItem(page, QStringLiteral("accessibilityReducedMotionSwitch"));
    auto *reducedTransparency =
        findItem(page, QStringLiteral("accessibilityReducedTransparencySwitch"));
    auto *slider = findItem(page, QStringLiteral("accessibilityTextScaleSlider"));
    auto *sample = findItem(page, QStringLiteral("accessibilityTextScaleSample"));
    auto *apply = findItem(page, QStringLiteral("accessibilityApplyButton"));
    auto *revert = findItem(page, QStringLiteral("accessibilityRevertButton"));
    QVERIFY(highContrast != nullptr);
    QVERIFY(reducedMotion != nullptr);
    QVERIFY(reducedTransparency != nullptr);
    QVERIFY(slider != nullptr);
    QVERIFY(sample != nullptr);
    QVERIFY(apply != nullptr);
    QVERIFY(revert != nullptr);
    const qreal basePointSize = pointSizeOf(sample);

    highContrast->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(highContrast, "toggled"));
    QCOMPARE(m_model->highContrastRequests, 1);
    QVERIFY(m_model->draftHighContrast);
    reducedMotion->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(reducedMotion, "toggled"));
    QCOMPARE(m_model->reducedMotionRequests, 1);
    reducedTransparency->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(reducedTransparency, "toggled"));
    QCOMPARE(m_model->reducedTransparencyRequests, 1);

    // Only an interactive move writes the draft; the sample scales live.
    slider->setProperty("value", 1.5);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QCOMPARE(m_model->textScaleRequests, 1);
    QCOMPARE(m_model->lastTextScale, 1.5);
    QTRY_COMPARE(pointSizeOf(sample), basePointSize * 1.5);

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
    QTRY_VERIFY(!highContrast->isEnabled());
    QVERIFY(!slider->isEnabled());
    QVERIFY(!apply->isEnabled());
    QVERIFY(!revert->isEnabled());
    QCOMPARE(apply->property("busy").toBool(), true);
}

void AccessibilityPageTest::presentsUnavailableWithRetry()
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
    auto *notice = findItem(page, QStringLiteral("accessibilityDegradedNotice"));
    auto *highContrast = findItem(page, QStringLiteral("accessibilityHighContrastSwitch"));
    auto *slider = findItem(page, QStringLiteral("accessibilityTextScaleSlider"));
    auto *retry = findItem(page, QStringLiteral("accessibilityRetryButton"));
    auto *status = findItem(page, QStringLiteral("accessibilityStatus"));
    QVERIFY(notice != nullptr);
    QTRY_VERIFY(notice->isVisible());
    const auto *noticeAccessible = QAccessible::queryAccessibleInterface(notice);
    QVERIFY(noticeAccessible != nullptr);
    QCOMPARE(noticeAccessible->role(), QAccessible::AlertMessage);
    QVERIFY(noticeAccessible->text(QAccessible::Description)
                .contains(QStringLiteral("unavailable")));
    QVERIFY(highContrast != nullptr);
    QVERIFY(!highContrast->isEnabled());
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

QTEST_MAIN(AccessibilityPageTest)
#include "tst_accessibility_page.moc"
