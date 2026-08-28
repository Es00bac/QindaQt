// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"
#include "state_card_accessibility_test.h"

#include "qindaqt/design_tokens/token_facade.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QColor>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <limits>
#include <memory>

using QindaQt::Controls::TestSupport::accessible;
using QindaQt::Controls::TestSupport::pinDeterministicFonts;
using QindaQt::Controls::TestSupport::publishTheme;
using QindaQt::Controls::TestSupport::completePreviewUsing;
using QindaQt::Controls::TestSupport::controlBackground;
using QindaQt::Controls::TestSupport::item;
using QindaQt::Controls::TestSupport::objectColor;
using QindaQt::Controls::TestSupport::waitForMotion;
using QindaQt::Controls::TestSupport::verifyStateCardAnnouncements;
using QindaQt::DesignTokens::AccessibilityInputs;
using QindaQt::DesignTokens::TokenFacade;

namespace {

struct Scene final {
    std::unique_ptr<QQuickView> view;
    QQuickItem *root = nullptr;
};

Scene createScene(const QString &theme,
                  const AccessibilityInputs &inputs = {},
                  const QSize &size = QSize(720, 1080))
{
    Scene scene{.view = std::make_unique<QQuickView>()};
    scene.view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    if (!publishTheme(*scene.view->engine(), theme, inputs, &error)) {
        qFatal("could not publish test tokens: %s", qPrintable(error));
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(size);
    scene.view->setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_CONTROLS_TEST_QML_DIR "/BehaviorScene.qml")));
    if (!scene.view->errors().isEmpty()) {
        qFatal("could not load control behavior scene: %s",
               qPrintable(scene.view->errors().constFirst().toString()));
    }
    scene.view->show();
    QCoreApplication::processEvents();
    scene.root = scene.view->rootObject();
    if (!scene.root) {
        qFatal("control behavior scene has no root item");
    }
    return scene;
}

} // namespace

class ControlsBehaviorTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void exposesSemanticStatesAndAccessibleRoles();
    void exposesStaticComponentContractsAndFocusRing();
    void activatesOrdinaryControlsFromKeyboard();
    void traversesOrdinaryControlsWithTab();
    void activatesCardActionsFromKeyboard();
    void busyButtonPreservesAvailabilityAndSuppressesActivation();
    void formRowForwardsLabelRequiredAndErrorSemantics();
    void stateCardAnnouncesDynamicSemanticTransitions();
    void rtlMirrorsSwitchAndSliderGeometry();
    void themeCardRejectsPartialAndHostilePreviews();
    void adaptsToEveryBuiltInTheme_data();
    void adaptsToEveryBuiltInTheme();
    void appliesReducedMotionAndTransparency();
    void longLocalizationUsesCompactFormFlow();
};

void ControlsBehaviorTests::initTestCase()
{
    pinDeterministicFonts();
}

void ControlsBehaviorTests::exposesSemanticStatesAndAccessibleRoles()
{
    auto scene = createScene(QStringLiteral("qinda-dark.json"));
    auto *primary = item(scene.root, "primaryButton");
    auto *busy = item(scene.root, "busyButton");
    auto *disabled = item(scene.root, "disabledButton");
    auto *check = item(scene.root, "checkBox");
    auto *slider = item(scene.root, "slider");
    auto *field = item(scene.root, "textField");
    auto *state = item(scene.root, "stateCard");
    auto *staticBusy = item(scene.root, "busyStateCard");
    auto *degraded = item(scene.root, "degradedNotice");
    auto *theme = item(scene.root, "themeCard");

    QCOMPARE(accessible(primary)->role(), QAccessible::Button);
    QCOMPARE(accessible(check)->role(), QAccessible::CheckBox);
    QCOMPARE(accessible(slider)->role(), QAccessible::Slider);
    QCOMPARE(accessible(field)->role(), QAccessible::EditableText);
    QCOMPARE(accessible(theme)->role(), QAccessible::RadioButton);
    QCOMPARE(accessible(state)->role(), QAccessible::AlertMessage);
    QCOMPARE(accessible(degraded)->role(), QAccessible::AlertMessage);
    QVERIFY(state->implicitWidth() >= 220.0);
    QVERIFY(staticBusy->implicitWidth() >= 220.0);
    QVERIFY(accessible(primary)->text(QAccessible::Name).contains(QStringLiteral("Apply")));
    QVERIFY(accessible(degraded)->text(QAccessible::Description)
                .contains(QStringLiteral("not running")));

    QVERIFY(!busy->isEnabled());
    QVERIFY(busy->property("busy").toBool());
    QVERIFY(accessible(busy)->text(QAccessible::Name).contains(QStringLiteral("busy")));
    QVERIFY(state->property("error").toBool());
    QVERIFY(!state->property("busy").toBool());
    QVERIFY(field->property("error").toBool());
    QCOMPARE(accessible(staticBusy)->role(), QAccessible::StaticText);
    QVERIFY(accessible(staticBusy)->text(QAccessible::Name).contains(QStringLiteral("busy")));
    QVERIFY(accessible(staticBusy)->text(QAccessible::Description)
                .contains(QStringLiteral("Wait while")));
    QVERIFY(!disabled->isEnabled());
    QVERIFY(!disabled->property("available").toBool());
    QVERIFY(accessible(disabled)->state().disabled);
    QVERIFY(accessible(disabled)->text(QAccessible::Description)
                .startsWith(QStringLiteral("Error.")));
}

void ControlsBehaviorTests::exposesStaticComponentContractsAndFocusRing()
{
    auto scene = createScene(QStringLiteral("qinda-dark.json"));
    auto *facade = scene.view->engine()->singletonInstance<TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    QVERIFY(facade != nullptr);

    auto *header = item(scene.root, "sectionHeader");
    QCOMPARE(accessible(header)->role(), QAccessible::StaticText);
    QCOMPARE(accessible(header)->text(QAccessible::Name),
             QStringLiteral("Accessible controls"));
    QVERIFY(accessible(header)->text(QAccessible::Description)
                .contains(QStringLiteral("keyboard")));

    auto *surface = item(scene.root, "formSurface");
    QCOMPARE(accessible(surface)->role(), QAccessible::Grouping);
    QCOMPARE(objectColor(controlBackground(surface)),
             facade->bg().value(QStringLiteral("raised")).value<QColor>());

    auto *label = item(scene.root, "specimenLabel");
    QCOMPARE(accessible(label)->role(), QAccessible::StaticText);
    QCOMPARE(accessible(label)->text(QAccessible::Name),
             QStringLiteral("Muted supporting label"));
    QCOMPARE(objectColor(label),
             facade->fg().value(QStringLiteral("muted")).value<QColor>());
    label->setEnabled(false);
    QCOMPARE(objectColor(label).rgba(),
             facade->fg()
                 .value(QStringLiteral("disabled"))
                 .value<QColor>()
                 .toRgb()
                 .rgba());

    auto *swatch = item(scene.root, "tokenSwatch");
    QCOMPARE(accessible(swatch)->role(), QAccessible::StaticText);
    QCOMPARE(accessible(swatch)->text(QAccessible::Name), QStringLiteral("Accent"));
    QVERIFY(accessible(swatch)->text(QAccessible::Description)
                .contains(QStringLiteral("semantic accent")));
    QCOMPARE(swatch->property("value").value<QColor>(),
             facade->accent().value(QStringLiteral("default")).value<QColor>());

    auto *primary = item(scene.root, "primaryButton");
    auto *focusRing = item(primary, "focusRing");
    item(scene.root, "textField")->forceActiveFocus();
    QCoreApplication::processEvents();
    QVERIFY(!focusRing->isVisible());
    primary->forceActiveFocus();
    QCoreApplication::processEvents();
    QVERIFY(primary->hasActiveFocus());
    QVERIFY(focusRing->property("focusVisible").toBool());
    QVERIFY(focusRing->isVisible());
    QCOMPARE(objectColor(focusRing), QColor(Qt::transparent));
}

void ControlsBehaviorTests::activatesOrdinaryControlsFromKeyboard()
{
    auto scene = createScene(QStringLiteral("qinda-light.json"));
    auto *primary = item(scene.root, "primaryButton");
    auto *check = item(scene.root, "checkBox");
    auto *toggle = item(scene.root, "switch");
    auto *slider = item(scene.root, "slider");
    auto *field = item(scene.root, "textField");
    auto *theme = item(scene.root, "themeCard");

    QSignalSpy primaryClicked(primary, SIGNAL(clicked()));
    primary->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QCOMPARE(primaryClicked.size(), 1);

    check->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QVERIFY(check->property("checked").toBool());

    toggle->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QVERIFY(toggle->property("checked").toBool());

    const double before = slider->property("value").toDouble();
    slider->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Right);
    QVERIFY(slider->property("value").toDouble() > before);

    field->forceActiveFocus();
    constexpr char input[] = "keyboard";
    for (const char key : input) {
        if (key != '\0') {
            QTest::keyClick(scene.view.get(), key);
        }
    }
    QCOMPARE(field->property("text").toString(), QStringLiteral("keyboard"));

    theme->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QVERIFY(theme->property("checked").toBool());
}

void ControlsBehaviorTests::traversesOrdinaryControlsWithTab()
{
    auto scene = createScene(QStringLiteral("qinda-light.json"));
    auto *primary = item(scene.root, "primaryButton");
    auto *field = item(scene.root, "textField");

    primary->forceActiveFocus();
    QVERIFY(primary->hasActiveFocus());
    QTest::keyClick(scene.view.get(), Qt::Key_Tab);
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::keyClick(scene.view.get(), Qt::Key_Tab, Qt::ShiftModifier);
    QTRY_VERIFY(primary->hasActiveFocus());
}

void ControlsBehaviorTests::activatesCardActionsFromKeyboard()
{
    auto scene = createScene(QStringLiteral("qinda-light.json"));
    auto *state = item(scene.root, "stateCard");
    auto *stateAction = item(state, "stateCardAction");
    QSignalSpy stateTriggered(state, SIGNAL(actionTriggered()));
    QCOMPARE(accessible(stateAction)->role(), QAccessible::Button);
    QCOMPARE(accessible(stateAction)->text(QAccessible::Name), QStringLiteral("Review"));
    stateAction->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QCOMPARE(stateTriggered.size(), 1);

    auto *degraded = item(scene.root, "degradedNotice");
    auto *retry = item(degraded, "stateCardAction");
    QSignalSpy retryRequested(degraded, SIGNAL(retryRequested()));
    QCOMPARE(accessible(retry)->role(), QAccessible::Button);
    QCOMPARE(accessible(retry)->text(QAccessible::Name), QStringLiteral("Retry"));
    retry->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QCOMPARE(retryRequested.size(), 1);
}

void ControlsBehaviorTests::busyButtonPreservesAvailabilityAndSuppressesActivation()
{
    auto scene = createScene(QStringLiteral("qinda-dusk.json"));
    auto *busy = item(scene.root, "busyButton");
    QSignalSpy clicked(busy, SIGNAL(clicked()));

    busy->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QCOMPARE(clicked.size(), 0);

    busy->setProperty("available", false);
    busy->setProperty("busy", false);
    QVERIFY(!busy->isEnabled());
    busy->setProperty("available", true);
    QVERIFY(busy->isEnabled());

    busy->forceActiveFocus();
    QTest::keyClick(scene.view.get(), Qt::Key_Space);
    QCOMPARE(clicked.size(), 1);
}

void ControlsBehaviorTests::formRowForwardsLabelRequiredAndErrorSemantics()
{
    auto scene = createScene(QStringLiteral("qinda-high-contrast.json"));
    auto *row = item(scene.root, "formRow");
    auto *field = item(scene.root, "textField");
    auto *editorHost = item(row, "formRowEditorHost");
    auto *interface = accessible(field);

    QVERIFY(row->property("wide").toBool());
    QVERIFY(editorHost->width() > 0.0);
    QVERIFY(editorHost->height() > 0.0);
    QVERIFY(editorHost->x() > 0.0);
    QCOMPARE(interface->role(), QAccessible::EditableText);
    QCOMPARE(field->property("accessibleName").toString(),
             QStringLiteral("Standalone editor name"));
    QCOMPARE(field->property("accessibleDescription").toString(),
             QStringLiteral("Standalone editor description"));
    QVERIFY(interface->text(QAccessible::Name).contains(QStringLiteral("required")));
    QCOMPARE(interface->text(QAccessible::Description),
             QStringLiteral("Error. Enter a valid value before continuing."));

    row->setProperty("errorMessage", QString());
    QCoreApplication::processEvents();
    QCOMPARE(interface->text(QAccessible::Description),
             QStringLiteral("This helper text also grows under localization."));

    row->setProperty("required", false);
    QCoreApplication::processEvents();
    QCOMPARE(interface->text(QAccessible::Name), scene.root->property("longLabel").toString());
}

void ControlsBehaviorTests::stateCardAnnouncesDynamicSemanticTransitions()
{
    auto scene = createScene(QStringLiteral("qinda-dark.json"));
    verifyStateCardAnnouncements(item(scene.root, "stateCard"));
}

void ControlsBehaviorTests::rtlMirrorsSwitchAndSliderGeometry()
{
    auto scene = createScene(QStringLiteral("qinda-macos.json"));
    scene.root->setProperty("rtl", true);
    QCoreApplication::processEvents();

    auto *toggle = item(scene.root, "switch");
    auto *track = item(toggle, "switchTrack");
    auto *knob = item(toggle, "switchKnob");
    QVERIFY(toggle->property("mirrored").toBool());
    toggle->setProperty("checked", false);
    waitForMotion(toggle);
    QVERIFY(knob->x() > (track->width() - knob->width()) / 2.0);
    toggle->setProperty("checked", true);
    waitForMotion(toggle);
    QVERIFY(knob->x() < (track->width() - knob->width()) / 2.0);

    auto *slider = item(scene.root, "slider");
    auto *sliderTrack = item(slider, "sliderTrack");
    auto *fill = item(slider, "progressFill");
    auto *handle = item(slider, "sliderHandle");
    QVERIFY(slider->property("mirrored").toBool());

    slider->setProperty("value", 25.0);
    waitForMotion(slider);
    QVERIFY(std::abs(fill->width() - sliderTrack->width() * 0.25) < 1.0);
    QVERIFY(std::abs(fill->x() - (sliderTrack->width() - fill->width())) < 1.0);
    QVERIFY(handle->x() > sliderTrack->width() / 2.0);

    slider->setProperty("value", 75.0);
    waitForMotion(slider);
    QVERIFY(std::abs(fill->width() - sliderTrack->width() * 0.75) < 1.0);
    QVERIFY(std::abs(fill->x() - (sliderTrack->width() - fill->width())) < 1.0);
    QVERIFY(handle->x() < sliderTrack->width() / 2.0);
}

void ControlsBehaviorTests::themeCardRejectsPartialAndHostilePreviews()
{
    auto scene = createScene(QStringLiteral("qinda-dark.json"));
    auto *theme = item(scene.root, "themeCard");
    auto *facade = scene.view->engine()->singletonInstance<TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    QVERIFY(facade != nullptr);

    QVERIFY(theme->property("previewValid").toBool());
    QVERIFY(!theme->property("previewUnavailable").toBool());
    QVERIFY(theme->isEnabled());

    theme->setProperty("previewTokens", QVariantMap{{QStringLiteral("bg"), facade->bg()}});
    QCoreApplication::processEvents();
    QVERIFY(!theme->property("previewValid").toBool());
    QVERIFY(theme->property("previewUnavailable").toBool());
    QVERIFY(!theme->isEnabled());
    QVERIFY(accessible(theme)->text(QAccessible::Description)
                .contains(QStringLiteral("Preview unavailable")));

    const auto requirePreviewRejected = [&](const QVariant &preview) {
        theme->setProperty("previewTokens", preview);
        QCoreApplication::processEvents();
        QVERIFY(!theme->property("previewValid").toBool());
        QVERIFY(theme->property("previewUnavailable").toBool());
        QVERIFY(!theme->isEnabled());
    };
    requirePreviewRejected(QStringLiteral("not-a-preview"));
    requirePreviewRejected(42);
    requirePreviewRejected(QVariantList{1, 2, 3});

    const auto requireRejected = [&](const QVariant &role) {
        theme->setProperty("previewTokens", completePreviewUsing(role));
        QCoreApplication::processEvents();
        QVERIFY(!theme->property("previewValid").toBool());
        QVERIFY(theme->property("previewUnavailable").toBool());
        QVERIFY(!theme->isEnabled());
    };
    requireRejected(QVariantMap{{QStringLiteral("a"), 1.0}});
    requireRejected(QVariantMap{{QStringLiteral("r"), QStringLiteral("0.2")},
                                {QStringLiteral("g"), 0.3},
                                {QStringLiteral("b"), 0.4},
                                {QStringLiteral("a"), 1.0}});
    requireRejected(QVariantMap{{QStringLiteral("r"),
                                 std::numeric_limits<double>::quiet_NaN()},
                                {QStringLiteral("g"), 0.3},
                                {QStringLiteral("b"), 0.4},
                                {QStringLiteral("a"), 1.0}});
    requireRejected(QVariantMap{{QStringLiteral("r"), 0.2},
                                {QStringLiteral("g"),
                                 std::numeric_limits<double>::infinity()},
                                {QStringLiteral("b"), 0.4},
                                {QStringLiteral("a"), 1.0}});
    requireRejected(QVariantMap{{QStringLiteral("r"), -0.1},
                                {QStringLiteral("g"), 0.3},
                                {QStringLiteral("b"), 0.4},
                                {QStringLiteral("a"), 1.1}});

    const QVariantMap complete = {
        {QStringLiteral("bg"), facade->bg()},
        {QStringLiteral("accent"), facade->accent()},
        {QStringLiteral("fg"), facade->fg()},
        {QStringLiteral("outline"), facade->outline()},
    };
    theme->setProperty("previewTokens", complete);
    QCoreApplication::processEvents();
    QVERIFY(theme->property("previewValid").toBool());
    QVERIFY(theme->isEnabled());
}

void ControlsBehaviorTests::adaptsToEveryBuiltInTheme_data()
{
    QTest::addColumn<QString>("themeFile");
    QTest::newRow("light") << QStringLiteral("qinda-light.json");
    QTest::newRow("dusk") << QStringLiteral("qinda-dusk.json");
    QTest::newRow("dark") << QStringLiteral("qinda-dark.json");
    QTest::newRow("high-contrast") << QStringLiteral("qinda-high-contrast.json");
    QTest::newRow("macos") << QStringLiteral("qinda-macos.json");
}

void ControlsBehaviorTests::adaptsToEveryBuiltInTheme()
{
    QFETCH(QString, themeFile);
    auto scene = createScene(themeFile);
    auto *facade = scene.view->engine()->singletonInstance<TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    auto *primary = item(scene.root, "primaryButton");
    QVERIFY(facade != nullptr);
    QVERIFY(facade->ready());
    QCOMPARE(objectColor(controlBackground(primary)),
             facade->accent().value(QStringLiteral("default")).value<QColor>());
}

void ControlsBehaviorTests::appliesReducedMotionAndTransparency()
{
    const AccessibilityInputs inputs{.reducedMotion = true,
                                     .reducedTransparency = true};
    auto scene = createScene(QStringLiteral("qinda-macos.json"), inputs);
    for (const char *name : {"primaryButton", "checkBox", "switch", "slider", "themeCard"}) {
        auto *control = item(scene.root, name);
        QVERIFY(control->property("transitionDuration").toInt() <= 80);
    }

    for (const char *name : {"primaryButton", "textField", "stateCard", "themeCard"}) {
        const QColor color = objectColor(controlBackground(item(scene.root, name)));
        QVERIFY2(color.isValid(), name);
        QCOMPARE(color.alpha(), 255);
    }
}

void ControlsBehaviorTests::longLocalizationUsesCompactFormFlow()
{
    for (const char *theme : {"qinda-light.json", "qinda-dusk.json", "qinda-dark.json",
                              "qinda-high-contrast.json", "qinda-macos.json"}) {
        auto scene = createScene(QString::fromLatin1(theme), {}, QSize(420, 1120));
        auto *row = item(scene.root, "formRow");
        auto *field = item(scene.root, "textField");
        auto *editorHost = item(row, "formRowEditorHost");
        QVERIFY2(!row->property("wide").toBool(), theme);
        QVERIFY2(row->implicitHeight() > field->implicitHeight(), theme);
        QVERIFY2(row->width() <= scene.root->width(), theme);
        QVERIFY2(editorHost->width() > 0.0, theme);
        QVERIFY2(editorHost->height() > 0.0, theme);
        QVERIFY2(editorHost->y() > 0.0, theme);
        QVERIFY2(field->width() > 0.0, theme);
        QVERIFY2(!accessible(field)->text(QAccessible::Name).isEmpty(), theme);

        auto *degraded = item(scene.root, "degradedNotice");
        auto *textColumn = item(degraded, "stateCardTextColumn");
        auto *title = item(degraded, "stateCardTitle");
        auto *message = item(degraded, "stateCardMessage");
        auto *action = item(degraded, "stateCardAction");
        QVERIFY2(textColumn->width() >= 160.0, theme);
        QVERIFY2(title->property("lineCount").toInt() <= 2, theme);
        QVERIFY2(message->width() >= 160.0, theme);
        QVERIFY2(action->isVisible(), theme);
        QVERIFY2(action->width() >= 96.0, theme);
    }
}

QTEST_MAIN(ControlsBehaviorTests)
#include "tst_controls_behavior.moc"
