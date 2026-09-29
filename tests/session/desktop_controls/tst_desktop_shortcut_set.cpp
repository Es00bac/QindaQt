// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/desktop_shortcut_set.h>

#include <QAction>
#include <QtTest>

using namespace QindaQt::Session::DesktopControls;

namespace {

class RecordingRegistrar final : public ShortcutRegistrar {
public:
    struct Record {
        QString actionId;
        QList<QKeySequence> defaultShortcuts;
        std::function<void(bool)> activeBindingChanged;
        ShortcutRegistration result;
    };

    ShortcutRegistration registerShortcut(QAction &action,
                                          const QList<QKeySequence> &defaultShortcuts,
                                          QObject &,
                                          std::function<void(bool)> activeBindingChanged) override
    {
        Record record;
        record.actionId = action.objectName();
        record.defaultShortcuts = defaultShortcuts;
        record.activeBindingChanged = std::move(activeBindingChanged);
        record.result = m_nextResult;
        m_records.append(record);
        return record.result;
    }

    void reportActiveBinding(const QString &actionId, bool present)
    {
        for (const Record &record : m_records) {
            if (record.actionId == actionId && record.activeBindingChanged) {
                record.activeBindingChanged(present);
            }
        }
    }

    // AGENT-GUARD: a record's activeBindingChanged callback captures `this`
    // of the DesktopShortcutSet that registered it. Once that set is
    // destroyed (every test function that shares this registrar constructs
    // its own, short-lived set), an unreset record from an earlier test is a
    // dangling callback; reportActiveBinding() would invoke it on freed
    // memory. Callers must reset() between test functions.
    void reset() { m_records.clear(); }

    [[nodiscard]] const QList<Record> &records() const noexcept { return m_records; }

    ShortcutRegistration m_nextResult{true, true};
    QList<Record> m_records;
};

int indexOf(const RecordingRegistrar &registrar, const QString &actionId)
{
    for (int index = 0; index < registrar.records().size(); ++index) {
        if (registrar.records().at(index).actionId == actionId) {
            return index;
        }
    }
    return -1;
}

} // namespace

class DesktopShortcutSetTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void registersEveryMediaKeyWithStableIdsAndDefaults();
    void triggersDispatchToTheMatchingCallback();
    void activeBindingChangesAreReportedPerAction();
    void rejectedRegistrationIsObservable();
    void brightnessRegistrationCanBeDisabledForPowerDevilOwnership();
    void screenshotRegistrationCanBeDisabledTogether();
    void screenshotKeysNeverTakeContainerShortcuts();
    void screenshotTriggersLaunchTheirOwnFlow();

private:
    RecordingRegistrar m_registrar;
};

void DesktopShortcutSetTest::init() {
    // Every DesktopShortcutSet constructed against m_registrar in one test
    // function is destroyed when that function returns; its captured
    // activeBindingChanged callbacks must not survive into the next test.
    m_registrar.reset();
}

void DesktopShortcutSetTest::registersEveryMediaKeyWithStableIdsAndDefaults() {
    const std::array<QPair<const char *, QList<QKeySequence>>,
                     static_cast<std::size_t>(DesktopShortcutAction::Count)>
        expected{{
            {"qindaqt_volume_up", {QKeySequence(Qt::Key_VolumeUp)}},
            {"qindaqt_volume_down", {QKeySequence(Qt::Key_VolumeDown)}},
            {"qindaqt_volume_mute", {QKeySequence(Qt::Key_VolumeMute)}},
            {"qindaqt_brightness_up", {QKeySequence(Qt::Key_MonBrightnessUp)}},
            {"qindaqt_brightness_down", {QKeySequence(Qt::Key_MonBrightnessDown)}},
            {"qindaqt_take_screenshot",
             {QKeySequence(Qt::Key_Print), QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Print)}},
            {"qindaqt_mic_mute", {QKeySequence(Qt::Key_MicMute)}},
            {"qindaqt_airplane_mode", {QKeySequence(Qt::Key_WLAN)}},
            {"qindaqt_screenshot_full_screen", {QKeySequence(Qt::SHIFT | Qt::Key_Print)}},
            {"qindaqt_screenshot_active_window", {QKeySequence(Qt::ALT | Qt::Key_Print)}},
            {"qindaqt_toggle_recording", {QKeySequence(Qt::META | Qt::ALT | Qt::Key_R)}},
        }};

    int volumeUps = 0;
    DesktopShortcutSet set(m_registrar,
                           DesktopShortcutTriggers{
                               .volumeUp = [&volumeUps] { ++volumeUps; },
                               .volumeDown = [] {},
                               .toggleMute = [] {},
                               .brightnessUp = [] {},
                               .brightnessDown = [] {},
                               .takeScreenshot = [] {},
                               .toggleMicMute = [] {},
                               .toggleAirplaneMode = [] {},
                           });

    QCOMPARE(m_registrar.records().size(),
             static_cast<int>(DesktopShortcutAction::Count));
    for (const auto &entry : expected) {
        const int position = indexOf(m_registrar, QLatin1String(entry.first));
        QVERIFY(position >= 0);
        QCOMPARE(m_registrar.records().at(position).defaultShortcuts, entry.second);
        QCOMPARE(DesktopShortcutSet::defaultShortcuts(
                     static_cast<DesktopShortcutAction>(position)),
                 entry.second);
        QCOMPARE(DesktopShortcutSet::stableActionId(
                     static_cast<DesktopShortcutAction>(position)),
                 QString::fromLatin1(entry.first));
    }
    QCOMPARE(volumeUps, 0);
}

void DesktopShortcutSetTest::triggersDispatchToTheMatchingCallback() {
    int volumeUps = 0;
    int mutes = 0;
    int prints = 0;
    int micMutes = 0;
    int airplaneToggles = 0;
    DesktopShortcutSet set(m_registrar,
                           DesktopShortcutTriggers{
                               .volumeUp = [&volumeUps] { ++volumeUps; },
                               .volumeDown = [] {},
                               .toggleMute = [&mutes] { ++mutes; },
                               .brightnessUp = [] {},
                               .brightnessDown = [] {},
                               .takeScreenshot = [&prints] { ++prints; },
                               .toggleMicMute = [&micMutes] { ++micMutes; },
                               .toggleAirplaneMode =
                                   [&airplaneToggles] { ++airplaneToggles; },
                           });

    emit set.action(DesktopShortcutAction::VolumeUp)->trigger();
    emit set.action(DesktopShortcutAction::ToggleMute)->trigger();
    emit set.action(DesktopShortcutAction::ToggleMute)->trigger();
    emit set.action(DesktopShortcutAction::TakeScreenshot)->trigger();
    emit set.action(DesktopShortcutAction::ToggleMicMute)->trigger();
    emit set.action(DesktopShortcutAction::ToggleAirplaneMode)->trigger();

    QCOMPARE(volumeUps, 1);
    QCOMPARE(mutes, 2);
    QCOMPARE(prints, 1);
    QCOMPARE(micMutes, 1);
    QCOMPARE(airplaneToggles, 1);
}

void DesktopShortcutSetTest::activeBindingChangesAreReportedPerAction() {
    DesktopShortcutSet set(m_registrar,
                           DesktopShortcutTriggers{
                               .volumeUp = [] {},
                               .volumeDown = [] {},
                               .toggleMute = [] {},
                               .brightnessUp = [] {},
                               .brightnessDown = [] {},
                               .takeScreenshot = [] {},
                               .toggleMicMute = [] {},
                               .toggleAirplaneMode = [] {},
                           });
    QVERIFY(set.activeBindingPresent(DesktopShortcutAction::VolumeUp));

    QSignalSpy spy(&set, &DesktopShortcutSet::activeBindingPresentChanged);
    m_registrar.reportActiveBinding(QStringLiteral("qindaqt_volume_up"), false);
    m_registrar.reportActiveBinding(QStringLiteral("qindaqt_take_screenshot"), false);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).value<DesktopShortcutAction>(),
             DesktopShortcutAction::VolumeUp);
    QCOMPARE(spy.at(0).at(1).toBool(), false);
    QCOMPARE(set.activeBindingPresent(DesktopShortcutAction::VolumeUp), false);
    QCOMPARE(set.activeBindingPresent(DesktopShortcutAction::TakeScreenshot), false);
    QCOMPARE(set.activeBindingPresent(DesktopShortcutAction::VolumeDown), true);
}

void DesktopShortcutSetTest::rejectedRegistrationIsObservable() {
    m_registrar.m_nextResult = ShortcutRegistration{false, false};
    DesktopShortcutSet set(m_registrar,
                           DesktopShortcutTriggers{
                               .volumeUp = [] {},
                               .volumeDown = [] {},
                               .toggleMute = [] {},
                               .brightnessUp = [] {},
                               .brightnessDown = [] {},
                               .takeScreenshot = [] {},
                               .toggleMicMute = [] {},
                               .toggleAirplaneMode = [] {},
                           });
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::VolumeUp));
    QVERIFY(!set.activeBindingPresent(DesktopShortcutAction::VolumeUp));
}

void DesktopShortcutSetTest::brightnessRegistrationCanBeDisabledForPowerDevilOwnership()
{
    RecordingRegistrar registrar;
    DesktopShortcutSet set(
        registrar,
        DesktopShortcutTriggers{
            .volumeUp = [] {},
            .volumeDown = [] {},
            .toggleMute = [] {},
            .brightnessUp = [] {},
            .brightnessDown = [] {},
            .takeScreenshot = [] {},
            .toggleMicMute = [] {},
            .toggleAirplaneMode = [] {},
        },
        nullptr,
        DesktopShortcutRegistrationOptions{.registerBrightness = false});

    QCOMPARE(registrar.records().size(),
             static_cast<int>(DesktopShortcutAction::Count) - 2);
    QCOMPARE(indexOf(registrar, QStringLiteral("qindaqt_brightness_up")), -1);
    QCOMPARE(indexOf(registrar, QStringLiteral("qindaqt_brightness_down")), -1);
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::BrightnessUp));
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::BrightnessDown));
}

void DesktopShortcutSetTest::screenshotRegistrationCanBeDisabledTogether()
{
    RecordingRegistrar registrar;
    DesktopShortcutSet set(
        registrar,
        DesktopShortcutTriggers{.volumeUp = [] {}, .volumeDown = [] {}, .toggleMute = [] {},
                                .brightnessUp = [] {}, .brightnessDown = [] {},
                                .takeScreenshot = [] {}, .toggleMicMute = [] {},
                                .toggleAirplaneMode = [] {}},
        nullptr,
        DesktopShortcutRegistrationOptions{.registerBrightness = true,
                                           .registerScreenshot = false});

    // An embedder with its own screenshot tool owns Print and the record
    // key together; none of the four may be registered half-way.
    QCOMPARE(registrar.records().size(),
             static_cast<int>(DesktopShortcutAction::Count) - 4);
    for (const char *id : {"qindaqt_take_screenshot", "qindaqt_screenshot_full_screen",
                           "qindaqt_screenshot_active_window", "qindaqt_toggle_recording"}) {
        QCOMPARE(indexOf(registrar, QLatin1String(id)), -1);
    }
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::TakeScreenshot));
    QVERIFY(!set.activeBindingPresent(DesktopShortcutAction::TakeScreenshot));
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::ToggleRecording));
}

void DesktopShortcutSetTest::screenshotKeysNeverTakeContainerShortcuts()
{
    // AGENT-GUARD (ADR-0289): Meta+Shift+S and Meta+Shift+R belong to the
    // container split/group resize actions; a default here would silently
    // unbind one of the two owners.
    const QKeySequence splitResize(Qt::META | Qt::SHIFT | Qt::Key_S);
    const QKeySequence groupResize(Qt::META | Qt::SHIFT | Qt::Key_R);
    for (int index = 0; index < static_cast<int>(DesktopShortcutAction::Count); ++index) {
        const auto keys = DesktopShortcutSet::defaultShortcuts(static_cast<DesktopShortcutAction>(index));
        QVERIFY(!keys.isEmpty());
        QVERIFY(!keys.contains(splitResize));
        QVERIFY(!keys.contains(groupResize));
    }
}

void DesktopShortcutSetTest::screenshotTriggersLaunchTheirOwnFlow()
{
    QStringList launched;
    // A private registrar: m_registrar keeps whatever m_nextResult an
    // earlier row left behind.
    RecordingRegistrar registrar;
    DesktopShortcutSet set(
        registrar,
        DesktopShortcutTriggers{
            .volumeUp = [] {},
            .volumeDown = [] {},
            .toggleMute = [] {},
            .brightnessUp = [] {},
            .brightnessDown = [] {},
            .takeScreenshot = [&launched] { launched.append(QStringLiteral("region")); },
            .toggleMicMute = [] {},
            .toggleAirplaneMode = [] {},
            .screenshotFullScreen = [&launched] { launched.append(QStringLiteral("full")); },
            .screenshotActiveWindow = [&launched] { launched.append(QStringLiteral("window")); },
            .toggleRecording = [&launched] { launched.append(QStringLiteral("record")); },
        });
    emit set.action(DesktopShortcutAction::ScreenshotActiveWindow)->trigger();
    emit set.action(DesktopShortcutAction::TakeScreenshot)->trigger();
    emit set.action(DesktopShortcutAction::ToggleRecording)->trigger();
    emit set.action(DesktopShortcutAction::ScreenshotFullScreen)->trigger();
    QCOMPARE(launched, (QStringList{QStringLiteral("window"), QStringLiteral("region"),
                                    QStringLiteral("record"), QStringLiteral("full")}));
    QVERIFY(set.registrationRequestAccepted(DesktopShortcutAction::ToggleRecording));
}

QTEST_MAIN(DesktopShortcutSetTest)
#include "tst_desktop_shortcut_set.moc"
