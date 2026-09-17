// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/smart_lights_store/configuration_store.h>
#include <qindaqt/services/smart_lights_store/smart_lights_configuration.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt::SmartLights;

namespace
{

[[nodiscard]] StoredConfiguration sampleConfiguration()
{
    StoredConfiguration configuration;

    StoredDevice device;
    device.mac = QStringLiteral("d8a011769356");
    device.label = QStringLiteral("Reading lamp");
    device.lastAddress = QStringLiteral("10.0.0.234");
    configuration.devices.append(device);

    PresetMember member;
    member.mac = device.mac;
    member.state.setPower = true;
    member.state.on = true;
    member.state.setDimming = true;
    member.state.dimmingPercent = 35;
    member.state.setTemperature = true;
    member.state.temperatureKelvin = 2700;

    StoredPreset preset;
    preset.id = QStringLiteral("evening");
    preset.name = QStringLiteral("Evening");
    preset.members.append(member);
    configuration.presets.append(preset);
    return configuration;
}

} // namespace

class SmartLightsStoreTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripsConfiguration();
    void refusesFutureSchemaVersions();
    void dropsUnusableEntriesWithoutLosingTheRest();
    void missingFileIsAnEmptyConfiguration();
    void savesAtomicallyAndReloads();
    void quarantinesAnUnreadableFileInsteadOfOverwritingIt();
    void makesUniquePresetIdentifiers();
};

void SmartLightsStoreTests::roundTripsConfiguration()
{
    const StoredConfiguration original = sampleConfiguration();
    const auto decoded = decodeConfiguration(encodeConfiguration(original));
    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, original);
}

void SmartLightsStoreTests::refusesFutureSchemaVersions()
{
    const QByteArray future = R"({"schemaVersion":99,"devices":[],"presets":[]})";
    QVERIFY(!decodeConfiguration(future).has_value());
    QVERIFY(!decodeConfiguration(QByteArray("{}")).has_value());
    QVERIFY(!decodeConfiguration(QByteArray("garbage")).has_value());
}

void SmartLightsStoreTests::dropsUnusableEntriesWithoutLosingTheRest()
{
    const QByteArray document = R"({
        "schemaVersion": 1,
        "devices": [
            {"mac": "not-a-mac", "label": "Broken"},
            {"mac": "d8a011769356", "label": "Good"}
        ],
        "presets": [
            {"id": "empty", "name": "Empty", "members": []},
            {"id": "ok", "name": "Ok", "members": [
                {"mac": "d8a011769356", "state": {"on": true, "brightness": 20}}
            ]}
        ]
    })";
    const auto decoded = decodeConfiguration(document);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->devices.size(), 1);
    QCOMPARE(decoded->devices.first().label, QStringLiteral("Good"));
    // A preset with nothing to replay is not a preset.
    QCOMPARE(decoded->presets.size(), 1);
    QCOMPARE(decoded->presets.first().id, QStringLiteral("ok"));
    QCOMPARE(decoded->presets.first().members.first().state.dimmingPercent, quint8{20});
}

void SmartLightsStoreTests::missingFileIsAnEmptyConfiguration()
{
    QTemporaryDir directory;
    ConfigurationStore store(QDir(directory.path()).filePath(QStringLiteral("none.json")));
    QString error;
    const StoredConfiguration configuration = store.load(&error);
    QVERIFY(configuration.devices.isEmpty());
    QVERIFY(!store.unreadable());
    QVERIFY(error.isEmpty());
}

void SmartLightsStoreTests::savesAtomicallyAndReloads()
{
    QTemporaryDir directory;
    const QString path =
        QDir(directory.path()).filePath(QStringLiteral("nested/smart-lights.json"));
    ConfigurationStore store(path);
    QVERIFY(store.save(sampleConfiguration()));
    QVERIFY(QFile::exists(path));

    ConfigurationStore reopened(path);
    QCOMPARE(reopened.load(), sampleConfiguration());
}

void SmartLightsStoreTests::quarantinesAnUnreadableFileInsteadOfOverwritingIt()
{
    QTemporaryDir directory;
    const QString path =
        QDir(directory.path()).filePath(QStringLiteral("smart-lights.json"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // A document from a newer desktop: valid JSON this build refuses.
        file.write(R"({"schemaVersion":99,"presets":[{"id":"x"}]})");
    }

    ConfigurationStore store(path);
    QString error;
    const StoredConfiguration loaded = store.load(&error);
    QVERIFY(loaded.presets.isEmpty());
    QVERIFY(store.unreadable());
    QVERIFY(!error.isEmpty());

    QVERIFY(store.save(sampleConfiguration()));
    // The user's unreadable file is preserved rather than destroyed.
    QVERIFY(QFile::exists(path + QStringLiteral(".invalid")));
    QCOMPARE(ConfigurationStore(path).load(), sampleConfiguration());
}

void SmartLightsStoreTests::makesUniquePresetIdentifiers()
{
    QList<StoredPreset> existing;
    const QString first = makePresetId(QStringLiteral("Evening  Wind Down!"), existing);
    QCOMPARE(first, QStringLiteral("evening-wind-down"));

    StoredPreset taken;
    taken.id = first;
    existing.append(taken);
    QCOMPARE(makePresetId(QStringLiteral("Evening Wind Down"), existing),
             QStringLiteral("evening-wind-down-2"));
    QVERIFY(!makePresetId(QStringLiteral("***"), existing).isEmpty());
}

QTEST_MAIN(SmartLightsStoreTests)
#include "tst_smart_lights_store.moc"
