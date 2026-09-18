// SPDX-License-Identifier: GPL-3.0-or-later
// The libobs half of the console bridge (ADR-0208), driven in-process on a
// headless libobs: no frontend, no D-Bus, real sources and mixer channels.
// Qt first: libobs's SIMDe native aliases collide with the compiler
// intrinsics that Qt's qsimd.h includes when they come afterwards.
#include <QSignalSpy>
#include <QtTest>

#include "support.h"

#include "bridge_controller.h"
#include "console_source_types.h"

#include <obs.h>

using namespace QindaQt;
using namespace QindaQt::ObsBridge;

namespace {

QString settingOf(obs_source_t *source, const char *key)
{
    obs_data_t *settings = obs_source_get_settings(source);
    const QString value = QString::fromUtf8(obs_data_get_string(settings, key));
    obs_data_release(settings);
    return value;
}

// The mixer channel a source sits on, or -1.
int channelOf(obs_source_t *source)
{
    for (uint32_t channel = 1; channel < MAX_CHANNELS; ++channel) {
        obs_source_t *current = obs_get_output_source(channel);
        const bool same = current == source;
        if (current != nullptr) {
            obs_source_release(current);
        }
        if (same) {
            return static_cast<int>(channel);
        }
    }
    return -1;
}

int bridgeSourceCount()
{
    int count = 0;
    obs_enum_sources(
        [](void *param, obs_source_t *source) {
            // Removed sources linger while a test reference holds them.
            if (!obs_source_removed(source)
                && sourceKindFromId(QString::fromUtf8(obs_source_get_unversioned_id(source)))) {
                ++*static_cast<int *>(param);
            }
            return true;
        },
        &count);
    return count;
}

} // namespace

class ObsBridgeLibobsTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void sourceTypesCreateWithoutACaptureBackend();
    void syncCreatesRenamesRetargetsAndRemoves();
    void restoredSourcesAreAdoptedNotDuplicated();
    void cleanupTestCase();
};

void ObsBridgeLibobsTests::initTestCase()
{
    QVERIFY2(obs_startup("en-US", nullptr, nullptr), "libobs did not start headless");
    struct obs_audio_info audio = {};
    audio.samples_per_sec = 48000;
    audio.speakers = SPEAKERS_STEREO;
    QVERIFY(obs_reset_audio(&audio));
    registerConsoleSourceTypes();
}

void ObsBridgeLibobsTests::sourceTypesCreateWithoutACaptureBackend()
{
    // No PulseAudio capture type is loaded here, so the child cannot exist;
    // the console source still does, silent, and keeps its settings.
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, SettingsKeys::ConsoleId, "bus.b1");
    obs_data_set_string(settings, SettingsKeys::CaptureKind, "input");
    obs_data_set_string(settings, SettingsKeys::CaptureDevice, "qindaqt.console.bus.b1.source");
    obs_source_t *source = obs_source_create(BusSourceId, "probe", settings, nullptr);
    obs_data_release(settings);
    QVERIFY(source != nullptr);
    QVERIFY(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO);
    QCOMPARE(QString::fromUtf8(obs_source_get_display_name(BusSourceId)),
             QStringLiteral("QindaQt Console Bus"));
    QCOMPARE(QString::fromUtf8(obs_source_get_display_name(StripSourceId)),
             QStringLiteral("QindaQt Console Strip"));
    QCOMPARE(settingOf(source, SettingsKeys::CaptureDevice),
             QStringLiteral("qindaqt.console.bus.b1.source"));
    obs_properties_t *properties = obs_source_properties(source);
    QVERIFY(obs_properties_get(properties, SettingsKeys::ConsoleId) != nullptr);
    obs_properties_destroy(properties);
    obs_source_remove(source);
    obs_source_release(source);
    QCOMPARE(bridgeSourceCount(), 0);
}

void ObsBridgeLibobsTests::syncCreatesRenamesRetargetsAndRemoves()
{
    BridgeController controller;
    QVERIFY(controller.frontendReady());
    QSignalSpy changed(&controller, &BridgeController::mappingChanged);
    Audio::Snapshot snapshot = consoleFixture();
    controller.applySnapshot(snapshot);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(bridgeSourceCount(), 7);
    QCOMPARE(controller.mapping().sources.size(), 7);
    QCOMPARE(controller.mapping().revision, quint64(42));

    obs_source_t *b1 = obs_get_source_by_name("QindaQt Bus B1 — Chat");
    QVERIFY(b1 != nullptr);
    QCOMPARE(QString::fromUtf8(obs_source_get_unversioned_id(b1)), QStringLiteral("qindaqt_console_bus"));
    QCOMPARE(settingOf(b1, SettingsKeys::ConsoleId), QStringLiteral("bus.b1"));
    QCOMPARE(settingOf(b1, SettingsKeys::CaptureDevice), QStringLiteral("qindaqt.console.bus.b1.source"));
    QVERIFY(channelOf(b1) >= static_cast<int>(BridgeController::FirstChannel));
    obs_source_t *music = obs_get_source_by_name("QindaQt Strip Virtual 1 — Music");
    QVERIFY(music != nullptr);
    QCOMPARE(QString::fromUtf8(obs_source_get_unversioned_id(music)),
             QStringLiteral("qindaqt_console_strip"));
    QVERIFY(channelOf(music) >= static_cast<int>(BridgeController::FirstChannel));
    QVERIFY(channelOf(music) != channelOf(b1));
    obs_source_release(music);

    // A rename in Settings renames the OBS source in place; a retarget
    // updates the capture node; a dropped bus removes its source and frees
    // its channel.
    snapshot.console.buses[2].label = QStringLiteral("Game");
    snapshot.console.buses[0].targetSerial = 102;
    snapshot.console.buses.removeAt(3);
    snapshot.revision = 43;
    controller.applySnapshot(snapshot);
    QCOMPARE(changed.size(), 2);
    QCOMPARE(bridgeSourceCount(), 6);
    QVERIFY(obs_get_source_by_name("QindaQt Bus B1 — Chat") == nullptr);
    obs_source_t *renamed = obs_get_source_by_name("QindaQt Bus B1 — Game");
    QVERIFY(renamed == b1);
    obs_source_release(renamed);
    obs_source_release(b1);
    obs_source_t *a1 = obs_get_source_by_name("QindaQt Bus A1 — Speakers");
    QVERIFY(a1 != nullptr);
    QCOMPARE(settingOf(a1, SettingsKeys::CaptureDevice),
             QStringLiteral("alsa_output.usb-dac.analog-stereo.monitor"));
    obs_source_release(a1);
    QVERIFY(obs_get_source_by_name("QindaQt Bus B2") == nullptr);

    // An empty console takes every bridge source with it.
    snapshot.console = {};
    controller.applySnapshot(snapshot);
    QCOMPARE(bridgeSourceCount(), 0);
    for (uint32_t channel = BridgeController::FirstChannel; channel < MAX_CHANNELS; ++channel) {
        obs_source_t *current = obs_get_output_source(channel);
        QVERIFY2(current == nullptr, "a removed source kept its mixer channel");
    }
}

void ObsBridgeLibobsTests::restoredSourcesAreAdoptedNotDuplicated()
{
    // A scene collection restores the bridge's sources before the bridge
    // syncs: they carry their settings but sit on no channel. The sync
    // adopts them instead of creating a second source.
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, SettingsKeys::ConsoleId, "bus.b1");
    obs_data_set_string(settings, SettingsKeys::Code, "B1");
    obs_data_set_string(settings, SettingsKeys::Label, "Old label");
    obs_data_set_string(settings, SettingsKeys::CaptureKind, "input");
    obs_data_set_string(settings, SettingsKeys::CaptureDevice, "qindaqt.console.bus.b1.source");
    obs_source_t *restored =
        obs_source_create(BusSourceId, "QindaQt Bus B1 — Old label", settings, nullptr);
    obs_data_release(settings);
    QVERIFY(restored != nullptr);
    QCOMPARE(channelOf(restored), -1);

    BridgeController controller;
    controller.applySnapshot(consoleFixture());
    QCOMPARE(bridgeSourceCount(), 7);
    obs_source_t *adopted = obs_get_source_by_name("QindaQt Bus B1 — Chat");
    QVERIFY(adopted == restored);
    QCOMPARE(settingOf(adopted, SettingsKeys::Label), QStringLiteral("Chat"));
    QVERIFY(channelOf(adopted) >= static_cast<int>(BridgeController::FirstChannel));
    obs_source_release(adopted);

    Audio::Snapshot empty;
    controller.applySnapshot(empty);
    QCOMPARE(bridgeSourceCount(), 0);
    obs_source_release(restored);
}

void ObsBridgeLibobsTests::cleanupTestCase()
{
    obs_shutdown();
}

QTEST_GUILESS_MAIN(ObsBridgeLibobsTests)
#include "tst_obs_bridge_libobs.moc"
