// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/console_model.h>

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Audio
{

// Named copies of the console document (ADR-0182), one file each under a
// directory of the user's own. A preset is exactly what the console store
// writes, under another name: nothing live, only the user's decisions.
class PresetStore final
{
public:
    explicit PresetStore(QString directory);
    // `$XDG_CONFIG_HOME/qindaqt/audio-presets`, or QINDAQT_AUDIO_PRESET_DIR.
    [[nodiscard]] static QString defaultDirectory();
    // The file name for a preset name: lower-case, [a-z0-9-], bounded.
    // Empty when nothing of the name survives, which also refuses a name that
    // could name a path.
    [[nodiscard]] static QString slugFor(const QString &name);

    // Preset names in stored (display) order, capped at kMaxPresets.
    [[nodiscard]] QStringList names() const;
    // False when the name is unusable, the cap is reached for a NEW name, or
    // the write fails. Saving an existing name replaces it.
    [[nodiscard]] bool save(const QString &name, const ConsoleModel &model);
    // False, with the model untouched, when there is no such preset or its
    // document is unusable.
    [[nodiscard]] bool load(const QString &name, ConsoleModel &model) const;
    [[nodiscard]] bool remove(const QString &name);

private:
    [[nodiscard]] QString pathFor(const QString &name) const;

    QString m_directory;
};

} // namespace QindaQt::Audio
