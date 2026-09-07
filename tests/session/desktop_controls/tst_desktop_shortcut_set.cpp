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
        QKeySequence defaultShortcut;
        std::function<void(bool)> activeBindingChanged;
        ShortcutRegistration result;
    };

    ShortcutRegistration registerShortcut(QAction &action,
                                          const QKeySequence &defaultShortcut,
                                          QObject &,
                                          std::function<void(bool)> activeBindingChanged) override
    {
        Record record;
        record.actionId = action.objectName();
        record.defaultShortcut = defaultShortcut;
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
    void registersEveryMediaKeyWithStableIdsAndDefaults();
    void triggersDispatchToTheMatchingCallback();
    void activeBindingChangesAreReportedPerAction();
    void rejectedRegistrationIsObservable();

private:
    RecordingRegistrar m_registrar;
};

void DesktopShortcutSetTest::registersEveryMediaKeyWithStableIdsAndDefaults() {
    const std::array<QPair<const char *, Qt::Key>,
                     static_cast<std::size_t>(DesktopShortcutAction::Count)>
        expected{{
            {"qindaqt_volume_up", Qt::Key_VolumeUp},
            {"qindaqt_volume_down", Qt::Key_VolumeDown},
            {"qindaqt_volume_mute", Qt::Key_VolumeMute},
            {"qindaqt_brightness_up", Qt::Key_MonBrightnessUp},
            {"qindaqt_brightness_down", Qt::Key_MonBrightnessDown},
            {"qindaqt_take_screenshot", Qt::Key_Print},
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
                           });

    QCOMPARE(m_registrar.records().size(),
             static_cast<int>(DesktopShortcutAction::Count));
    for (const auto &entry : expected) {
        const int position = indexOf(m_registrar, QLatin1String(entry.first));
        QVERIFY(position >= 0);
        QCOMPARE(m_registrar.records().at(position).defaultShortcut,
                 QKeySequence(entry.second));
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
    DesktopShortcutSet set(m_registrar,
                           DesktopShortcutTriggers{
                               .volumeUp = [&volumeUps] { ++volumeUps; },
                               .volumeDown = [] {},
                               .toggleMute = [&mutes] { ++mutes; },
                               .brightnessUp = [] {},
                               .brightnessDown = [] {},
                               .takeScreenshot = [&prints] { ++prints; },
                           });

    emit set.action(DesktopShortcutAction::VolumeUp)->trigger();
    emit set.action(DesktopShortcutAction::ToggleMute)->trigger();
    emit set.action(DesktopShortcutAction::ToggleMute)->trigger();
    emit set.action(DesktopShortcutAction::TakeScreenshot)->trigger();

    QCOMPARE(volumeUps, 1);
    QCOMPARE(mutes, 2);
    QCOMPARE(prints, 1);
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
                           });
    QVERIFY(!set.registrationRequestAccepted(DesktopShortcutAction::VolumeUp));
    QVERIFY(!set.activeBindingPresent(DesktopShortcutAction::VolumeUp));
}

QTEST_MAIN(DesktopShortcutSetTest)
#include "tst_desktop_shortcut_set.moc"
