// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// VBAN streams (ADR-0185) are defined in a user-owned document, read
// fail-closed, like macro buttons: the Audio route's closed surface has no
// text entry for a host name, so hosts and stream names live in a file and
// Settings switches them on and off.
//
//   { "outgoing": [ { "name": "Stream1", "bus": "bus.a2", "host": "192.168.1.20", "port": 6980 } ],
//     "incoming": [ { "name": "Laptop", "port": 6980 } ] }
class VbanStore final
{
public:
    explicit VbanStore(QString path);
    // `$XDG_CONFIG_HOME/qindaqt/audio-vban.json`, or QINDAQT_AUDIO_VBAN_PATH.
    [[nodiscard]] static QString defaultPath();
    // The defined streams with `enabled` and `active` false; a stream whose
    // fields do not parse is dropped, names are unique (first wins), at most
    // kMaxVbanStreams.
    [[nodiscard]] QList<VbanStream> load() const;

private:
    QString m_path;
};

} // namespace QindaQt::Audio
