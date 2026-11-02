// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
namespace qindaqt::keyring::protocol {
// QMP2 is a private daemon/helper pipe contract; deploy both together. All
// integers are big endian: PID/UID u32, five u16-length UTF-8 strings in member
// order, u8 parent count, then u16-length parents. No trailing/NUL bytes.
// Process metadata is observed context, never executable or intent attestation.
inline constexpr qsizetype MaxPromptMetadataBytes = 12288;
struct PromptMetadata {
    QString label, caller, executable, operation, application;
    quint32 processId = 0, userId = 0;
    QStringList parents;
};
QByteArray encodePromptMetadata(const PromptMetadata &);
PromptMetadata decodePromptMetadata(QByteArray &);
// Display controls/bidi as visible escapes; callers must still use PlainText.
QString promptDisplayText(const QString &);
}
