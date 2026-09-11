// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/night_light/night_light_values.h>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

namespace QindaQt::Services::NightLight {

// Reads and writes the two persisted night-light files:
//
// - `kwinrc` group `NightColor`: Active, Mode, DayTemperature,
//   NightTemperature. KWin's nightlight plugin watches this file and applies
//   changes live.
// - `knighttimerc` groups `General`/`Location`/`Times`: Source, Automatic,
//   Latitude, Longitude, SunriseStart, SunsetStart, TransitionDuration.
//   knighttimed watches this file through KDE's config-watch machinery, so
//   schedule writes apply live while the daemon runs.
//
// AGENT-GUARD: The port must preserve every unrelated group and key in both
// files and must not write a value that already reads back equal. KWin and
// knighttimed treat these files as their own live configuration; a lossy or
// chatty writer would corrupt unrelated desktop configuration.
//
// Both paths are injected by the composition root; the port never resolves
// HOME, XDG variables, or standard locations itself, so tests can stage
// disposable files.
class NightLightConfigPort : public QObject {
    Q_OBJECT

public:
    enum class ReadOutcome {
        // Both files (or their groups) were absent or incomplete: the result
        // carries defaults for every absent entry.
        Absent,
        Loaded,
        // A stored value is hostile (unknown token, out-of-bounds number).
        // The result carries defaults; callers must not publish it as truth.
        Failed,
    };

    struct ReadResult {
        ReadOutcome outcome = ReadOutcome::Absent;
        NightLightSettings values;
        QString diagnostic;
    };

    enum class WriteOutcome {
        Applied,
        // Every desired value already reads back equal; no file was touched.
        Unchanged,
        Failed,
    };

    struct WriteResult {
        WriteOutcome outcome = WriteOutcome::Unchanged;
        QString diagnostic;
    };

    explicit NightLightConfigPort(QObject *parent = nullptr);
    ~NightLightConfigPort() override = default;

    virtual ReadResult read() const = 0;
    virtual WriteResult write(const NightLightSettings &settings) = 0;

Q_SIGNALS:
    // Emitted when a file changed from outside this port. Self-writes are
    // suppressed, so subscribers can treat this as external intent.
    void changedExternally();
};

// Production implementation over plain INI files (the KConfig dialect).
// Value shapes are exactly what KConfigXT reads: bool true/false, enum
// choice-name strings, "HH:mm:ss" times, C-locale doubles.
class QtConfigNightLightPort final : public NightLightConfigPort {
    Q_OBJECT

public:
    explicit QtConfigNightLightPort(QString kwinRcPath,
                                    QString knightTimeRcPath,
                                    QObject *parent = nullptr);
    ~QtConfigNightLightPort() override;

    ReadResult read() const override;
    WriteResult write(const NightLightSettings &settings) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::NightLight
