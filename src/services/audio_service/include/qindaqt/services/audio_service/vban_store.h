// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// Audio1-owned manual-peer definitions (ADR-0246). Reads are value copies;
// writes validate a complete definition and atomically replace the document.
// An incoming definition requires an exact source IPv4 and output node name.
// Lifetime/threading: this value store is used serially by the coordinator on
// its Qt thread; the path is fixed at construction and no watcher is owned.
class VbanStore final
{
public:
    explicit VbanStore(QString path);
    // `$XDG_CONFIG_HOME/qindaqt/audio-vban.json`, or QINDAQT_AUDIO_VBAN_PATH.
    [[nodiscard]] static QString defaultPath();
    // Invalid or legacy-open incoming entries are never admitted. Missing file
    // means an empty definition set; malformed JSON also loads empty.
    [[nodiscard]] QList<VbanStream> load() const;
    // Definition edits do not modify enabled intent; callers republish after
    // success. Failure leaves the previous file intact and supplies a stable
    // reason code suitable for Audio1 OperationResult.
    [[nodiscard]] bool upsert(const VbanStream &definition, QString *reasonCode) const;
    [[nodiscard]] bool remove(const QString &name, QString *reasonCode) const;

private:
    QString m_path;
};

} // namespace QindaQt::Audio
