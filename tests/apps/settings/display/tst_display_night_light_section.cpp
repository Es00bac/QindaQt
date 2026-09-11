// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"
#include "display_page_test_support.h"
#include "stub_night_light_model.h"

#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

namespace {

namespace PageSupport = QindaQt::Tests::DisplayPageSupport;
using QindaQt::Tests::DisplayPageSupport::findItemByObjectName;
using QindaQt::Tests::DisplayPageSupport::StubNightLightModel;

const char *const BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
const char *const SectionQmlPath = QINDAQT_NIGHT_LIGHT_SECTION_QML_PATH;

struct LoadedSection final {
    std::unique_ptr<QObject> object;
    QQuickItem *item = nullptr;
};

LoadedSection loadSection(QQmlEngine &engine, StubNightLightModel &model)
{
    QQmlComponent component(&engine);
    component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(SectionQmlPath)));
    if (!component.isReady()) {
        qWarning() << component.errorString();
        return {};
    }
    QObject *created = component.createWithInitialProperties(
        {{QStringLiteral("nightLight"),
          QVariant::fromValue(static_cast<QObject *>(&model))}});
    if (created == nullptr) {
        return {};
    }
    LoadedSection section;
    section.object.reset(created);
    section.item = qobject_cast<QQuickItem *>(created);
    if (section.item == nullptr) {
        qWarning("night light section root is not an Item");
        return {};
    }
    return section;
}

} // namespace

class DisplayNightLightSectionTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    // Bindings: section widgets reflect model truth and drafts.
    void bindingsFollowTheModel();
    // Unavailable state: notice visible, every control disabled.
    void unavailableStateDisablesAndExplains();
    // Preview/stopPreview: one call per settled value, withdrawn on release.
    void previewIsDebouncedAndWithdrawn();
    // Keyboard: switch, combo, and slider are reachable and operable.
    void keyboardOperation();
    // Page close withdraws an in-flight preview.
    void closingWithdrawsPreview();

private:
    std::unique_ptr<QQuickView> m_view;
};

void DisplayNightLightSectionTest::initTestCase()
{
    m_view = std::make_unique<QQuickView>();
    m_view->engine()->addImportPath(QString::fromUtf8(BuildQmlImportPath));

    QString facadeError;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *m_view->engine(), &facadeError);
    QVERIFY2(facade != nullptr, qPrintable(facadeError));

    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QString pubError;
    QVERIFY2(facade->publish(loaded.theme, {}, &pubError),
             qPrintable(pubError));
}

void DisplayNightLightSectionTest::bindingsFollowTheModel()
{
    StubNightLightModel model;
    LoadedSection section = loadSection(*m_view->engine(), model);
    QVERIFY(section.item != nullptr);

    PageSupport::attachPage(*m_view, *section.item);

    auto *status = findItemByObjectName(section.item,
                                        QStringLiteral("nightLightStatusLabel"));
    QVERIFY(status != nullptr);
    QVERIFY(status->isVisible());
    QCOMPARE(status->property("text").toString(), model.statusText);

    auto *slider = findItemByObjectName(
        section.item, QStringLiteral("nightLightTemperatureSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->property("value").toDouble(), 3400.0);

    auto *combo = findItemByObjectName(section.item,
                                       QStringLiteral("nightLightScheduleCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->property("currentIndex").toInt(), 0);

    model.draftNightTemperature = 4500;
    Q_EMIT model.draftChanged();
    QTRY_COMPARE(slider->property("value").toDouble(), 4500.0);
    QTRY_COMPARE(slider->property("enabled").toBool(), true);

    auto *valueLabel = findItemByObjectName(
        section.item, QStringLiteral("nightLightTemperatureValue"));
    QVERIFY(valueLabel != nullptr);
    QTRY_VERIFY(valueLabel->property("text").toString().contains(
        QStringLiteral("4500")));

    model.statusText = QStringLiteral("Off — 6500 K.");
    Q_EMIT model.truthChanged();
    QTRY_COMPARE(status->property("text").toString(), model.statusText);
}

void DisplayNightLightSectionTest::unavailableStateDisablesAndExplains()
{
    StubNightLightModel model;
    model.available = false;
    model.statusText = QStringLiteral("Night light status is unknown right now.");
    LoadedSection section = loadSection(*m_view->engine(), model);
    QVERIFY(section.item != nullptr);

    PageSupport::attachPage(*m_view, *section.item);

    auto *notice = findItemByObjectName(
        section.item, QStringLiteral("nightLightUnavailableNotice"));
    QVERIFY(notice != nullptr);
    QVERIFY(notice->isVisible());

    auto *status = findItemByObjectName(section.item,
                                        QStringLiteral("nightLightStatusLabel"));
    QVERIFY(status != nullptr);
    QVERIFY(!status->isVisible());

    // Every control disables: the switch, the schedule combo, both sliders.
    const QStringList disabledControls = {
        QStringLiteral("nightLightEnableSwitch"),
        QStringLiteral("nightLightScheduleCombo"),
        QStringLiteral("nightLightTemperatureSlider"),
        QStringLiteral("nightLightDayTemperatureSlider"),
    };
    for (const QString &name : disabledControls) {
        auto *control = findItemByObjectName(section.item, name);
        QVERIFY2(control != nullptr, qPrintable(name));
        QVERIFY2(!control->property("enabled").toBool(),
                 qPrintable(QStringLiteral("%1 must be disabled")
                                .arg(name)));
    }
}

void DisplayNightLightSectionTest::previewIsDebouncedAndWithdrawn()
{
    StubNightLightModel model;
    LoadedSection section = loadSection(*m_view->engine(), model);
    QVERIFY(section.item != nullptr);

    PageSupport::attachPage(*m_view, *section.item);

    auto *slider = findItemByObjectName(
        section.item, QStringLiteral("nightLightTemperatureSlider"));
    QVERIFY(slider != nullptr);
    // The drag-end withdrawal hooks fire on a focus edge, so the slider must
    // actually hold focus while it is "dragged".
    slider->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(slider->hasActiveFocus());

    // Three rapid moves settle into exactly one preview call. A real drag
    // changes the value before each moved() emission, so set it explicitly.
    const auto dragTo = [&slider](double value) {
        slider->setProperty("value", value);
        QMetaObject::invokeMethod(slider, "moved");
    };
    dragTo(3500.0);
    QTest::qWait(50);
    dragTo(3600.0);
    QTest::qWait(50);
    dragTo(3700.0);
    QCOMPARE(model.previewTemperatures.size(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(model.previewTemperatures.size() == 1, 2'000);
    QCOMPARE(model.previewTemperatures.constFirst(), 3700.0);
    QCOMPARE(model.draftNightTemperature, 3700);

    // Moving focus away withdraws the preview (keyboard end): no
    // temperature lingers after the user moves on.
    auto *enableSwitch = findItemByObjectName(
        section.item, QStringLiteral("nightLightEnableSwitch"));
    QVERIFY(enableSwitch != nullptr);
    enableSwitch->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(!slider->hasActiveFocus());
    QTRY_VERIFY_WITH_TIMEOUT(model.stopPreviewCalls >= 1, 2'000);
}

void DisplayNightLightSectionTest::keyboardOperation()
{
    StubNightLightModel model;
    LoadedSection section = loadSection(*m_view->engine(), model);
    QVERIFY(section.item != nullptr);

    PageSupport::attachPage(*m_view, *section.item);

    // Switch: space toggles.
    auto *enableSwitch = findItemByObjectName(
        section.item, QStringLiteral("nightLightEnableSwitch"));
    QVERIFY(enableSwitch != nullptr);
    enableSwitch->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(enableSwitch->hasActiveFocus());
    const bool wasChecked = enableSwitch->property("checked").toBool();
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QTRY_COMPARE(enableSwitch->property("checked").toBool(), !wasChecked);
    QCOMPARE(model.draftActive, !wasChecked);
    // Restore the on draft: the preview path is gated on an active draft.
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QTRY_COMPARE(enableSwitch->property("checked").toBool(), wasChecked);
    QCOMPARE(model.draftActive, wasChecked);

    // Combo: focus, then open and move to the next entry with the keyboard.
    auto *combo = findItemByObjectName(section.item,
                                       QStringLiteral("nightLightScheduleCombo"));
    QVERIFY(combo != nullptr);
    combo->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(combo->hasActiveFocus());
    QTest::keyClick(m_view.get(), Qt::Key_Down);
    QTest::keyClick(m_view.get(), Qt::Key_Return);
    QTRY_COMPARE(model.draftScheduleMode, 1);
    QVERIFY(findItemByObjectName(section.item,
                                 QStringLiteral("nightLightLocationRow"))
                ->isVisible());

    // Slider: arrow keys step by 100 K and settle a preview.
    auto *slider = findItemByObjectName(
        section.item, QStringLiteral("nightLightTemperatureSlider"));
    QVERIFY(slider != nullptr);
    slider->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(slider->hasActiveFocus());
    // AGENT-NOTE: Qt 6.11 treats each keyboard step as one press/release
    // gesture, so the settled preview of a keyboard step is withdrawn at the
    // step's release edge, exactly like a released pointer drag. The value
    // feedback stays immediate; no preview lingers.
    QTest::keyClick(m_view.get(), Qt::Key_Left);
    QTRY_COMPARE(model.draftNightTemperature, 3300);
    QTRY_VERIFY_WITH_TIMEOUT(model.stopPreviewCalls >= 1, 2'000);
    QTest::keyClick(m_view.get(), Qt::Key_Right);
    QTRY_COMPARE(model.draftNightTemperature, 3400);
    QTRY_VERIFY_WITH_TIMEOUT(model.stopPreviewCalls >= 2, 2'000);
    QCOMPARE(model.previewTemperatures.size(), 0);
}

void DisplayNightLightSectionTest::closingWithdrawsPreview()
{
    StubNightLightModel model;
    LoadedSection section = loadSection(*m_view->engine(), model);
    QVERIFY(section.item != nullptr);

    PageSupport::attachPage(*m_view, *section.item);

    auto *slider = findItemByObjectName(
        section.item, QStringLiteral("nightLightTemperatureSlider"));
    QVERIFY(slider != nullptr);
    slider->setProperty("value", 3800.0);
    QMetaObject::invokeMethod(slider, "moved");
    QTRY_VERIFY_WITH_TIMEOUT(model.previewTemperatures.size() == 1, 2'000);

    // Page close destroys the section; the in-flight preview is withdrawn.
    section.object.reset();
    QTRY_VERIFY_WITH_TIMEOUT(model.stopPreviewCalls >= 1, 2'000);
}

QTEST_MAIN(DisplayNightLightSectionTest)
#include "tst_display_night_light_section.moc"
