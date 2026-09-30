// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
namespace qindaqt::keyring::protocol {
// Non-password presentation values, copied only for one prompt lifetime.
// Frame QMP1/u16 label/u16 caller/UTF-8 payload; no trailing or NUL bytes.
struct PromptMetadata { QString label,caller; };
QByteArray encodePromptMetadata(const PromptMetadata &);
PromptMetadata decodePromptMetadata(QByteArray &);
}
