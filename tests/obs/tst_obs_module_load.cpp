// SPDX-License-Identifier: GPL-3.0-or-later
// The built obs-qindaqt module loads into a headless libobs the way OBS
// loads it: through obs_open_module/obs_init_module, registering both source
// types (ADR-0208).
// Qt first: libobs's SIMDe native aliases collide with the compiler
// intrinsics that Qt's qsimd.h includes when they come afterwards.
#include <QtTest>

#include <obs.h>

class ObsModuleLoadTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moduleLoadsAndRegistersTheConsoleSourceTypes();
};

void ObsModuleLoadTests::moduleLoadsAndRegistersTheConsoleSourceTypes()
{
    QVERIFY2(obs_startup("en-US", nullptr, nullptr), "libobs did not start headless");
    obs_module_t *module = nullptr;
    QCOMPARE(obs_open_module(&module, QINDAQT_OBS_MODULE_PATH, nullptr), MODULE_SUCCESS);
    QVERIFY(module != nullptr);
    QVERIFY(obs_init_module(module));
    QCOMPARE(QString::fromUtf8(obs_get_module_name(module)), QStringLiteral("obs-qindaqt"));

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "qindaqt_console_id", "strip.virtual.1");
    obs_data_set_string(settings, "qindaqt_capture_kind", "monitor");
    obs_data_set_string(settings, "qindaqt_capture_device", "qindaqt.console.strip.virtual.1.monitor");
    obs_source_t *strip = obs_source_create("qindaqt_console_strip", "probe strip", settings, nullptr);
    obs_source_t *bus = obs_source_create("qindaqt_console_bus", "probe bus", settings, nullptr);
    obs_data_release(settings);
    QVERIFY(strip != nullptr);
    QVERIFY(bus != nullptr);
    QVERIFY(obs_source_get_output_flags(strip) & OBS_SOURCE_AUDIO);
    QVERIFY(obs_source_get_output_flags(bus) & OBS_SOURCE_AUDIO);
    // Hidden from the "add source" menu: the bridge owns their lifecycle.
    QVERIFY(obs_source_get_output_flags(bus) & OBS_SOURCE_CAP_DISABLED);
    obs_source_remove(strip);
    obs_source_remove(bus);
    obs_source_release(strip);
    obs_source_release(bus);
    obs_shutdown();
}

QTEST_GUILESS_MAIN(ObsModuleLoadTests)
#include "tst_obs_module_load.moc"
