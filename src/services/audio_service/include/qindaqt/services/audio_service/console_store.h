// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/console_model.h>

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// The console's configuration on disk (ADR-0176). One JSON document, written
// atomically, read fail-closed.
//
// AGENT-CONTRACT: the document carries only the user's decisions - labels,
// gains, mutes, routing - never live device handles or meter readings, which
// is exactly what ConsoleModel::toJson already restricts itself to. A store
// that persisted a serial would bind the console to a device that no longer
// exists after every reboot.
class ConsoleStore final
{
public:
    // A document larger than this is not a console: it is refused unread, so
    // a corrupted or hostile file cannot make the service allocate on its
    // behalf.
    static constexpr qint64 kMaxDocumentBytes = 256 * 1024;

    explicit ConsoleStore(QString path);

    // `$XDG_CONFIG_HOME/qindaqt/audio-console.json`, beside the settings
    // service's own document.
    [[nodiscard]] static QString defaultPath();
    [[nodiscard]] const QString &path() const noexcept { return m_path; }

    // Loads the document into the model. False, with the model untouched,
    // when there is no file, the file is oversized, or it is not a JSON
    // object; a partially usable document is applied through the model's own
    // per-entry tolerance.
    [[nodiscard]] bool load(ConsoleModel &model) const;
    // Writes the model atomically. A document identical to the last one this
    // store wrote is not rewritten, so a caller may save on every console
    // change without wearing the disk on graph churn.
    [[nodiscard]] bool save(const ConsoleModel &model);

private:
    QString m_path;
    QByteArray m_lastWritten;
};

} // namespace QindaQt::Audio
