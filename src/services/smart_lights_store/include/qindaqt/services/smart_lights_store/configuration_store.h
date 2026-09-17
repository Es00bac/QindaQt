// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/smart_lights_store/smart_lights_configuration.h>

#include <QtCore/QString>

namespace QindaQt::SmartLights
{

// File-backed persistence for the desktop's own smart-light configuration.
//
// AGENT-CONTRACT: this is the only component in the smart-light stack that
// touches the filesystem. Writes are atomic (QSaveFile), so an interrupted
// session cannot leave a half-written preset list behind.
//
// AGENT-GUARD: when an existing file cannot be decoded, the store refuses to
// overwrite it in place. It moves it aside to `<path>.invalid` on the first
// successful save, because a file this build cannot read may be a newer
// version's, and silently replacing it would destroy the user's presets.
class ConfigurationStore
{
public:
    explicit ConfigurationStore(QString path);

    // GenericConfigLocation/qindaqt/smart-lights.json, matching where the rest
    // of the desktop keeps per-user state.
    [[nodiscard]] static QString defaultPath();

    [[nodiscard]] QString path() const { return m_path; }

    // A missing file is not an error: it decodes to an empty configuration.
    // `error` is set only when a file exists and could not be used.
    [[nodiscard]] StoredConfiguration load(QString *error = nullptr);

    [[nodiscard]] bool save(const StoredConfiguration &configuration,
                            QString *error = nullptr);

    // True when the file on disk existed but could not be decoded. Callers
    // should surface this rather than presenting an empty list as the truth.
    [[nodiscard]] bool unreadable() const noexcept { return m_unreadable; }

private:
    QString m_path;
    bool m_unreadable = false;
};

} // namespace QindaQt::SmartLights
