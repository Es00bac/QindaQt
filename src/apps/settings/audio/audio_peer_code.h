// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace QindaQt::Apps::SettingsAudio {

struct PeerCode {
  QString name;
  QString sourceIpv4;
  quint32 port = 0;
};

// Settings-only transfer format. It conveys no receive permission or trust.
[[nodiscard]] QString encodePeerCode(const PeerCode &peer);
[[nodiscard]] bool decodePeerCode(const QString &text, PeerCode *peer,
                                  QString *reason);

} // namespace QindaQt::Apps::SettingsAudio
