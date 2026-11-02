// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <QScopeGuard>
#include <QStringDecoder>
#include <stdexcept>
namespace qindaqt::keyring::protocol {
namespace {
[[noreturn]] void invalid() { throw std::runtime_error("Invalid prompt metadata"); }
QString utf8(QByteArrayView bytes) {
    QStringDecoder decode(QStringDecoder::Utf8);
    const QString value = decode(bytes);
    if (decode.hasError() || value.contains(QChar(0))) invalid();
    return value;
}
void number(QByteArray &out, quint32 value, int bytes) {
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8)
        out.append(static_cast<char>((value >> shift) & 255U));
}
void string(QByteArray &out, const QString &value, qsizetype maximum) {
    auto bytes = value.toUtf8();
    auto scrub = qScopeGuard([&] { wipe(bytes); });
    if (bytes.size() > maximum || utf8(bytes) != value) invalid();
    number(out, static_cast<quint32>(bytes.size()), 2);
    out.append(bytes);
}
class Reader {
public:
    QByteArrayView bytes;
    quint32 number(int size) {
        if (bytes.size() < size) invalid();
        quint32 value = 0;
        for (int i = 0; i < size; ++i) value = (value << 8) | static_cast<unsigned char>(bytes[i]);
        bytes = bytes.sliced(size);
        return value;
    }
    QString string(qsizetype maximum) {
        const auto size = static_cast<qsizetype>(number(2));
        if (size > maximum || size > bytes.size()) invalid();
        const auto value = utf8(bytes.first(size));
        bytes = bytes.sliced(size);
        return value;
    }
};
}
QByteArray encodePromptMetadata(const PromptMetadata &value) {
    if (value.parents.size() > 6) invalid();
    QByteArray out("QMP2", 4);
    number(out, value.processId, 4); number(out, value.userId, 4);
    string(out, value.label, 1024); string(out, value.caller, 256);
    string(out, value.executable, 4096); string(out, value.operation, 32);
    string(out, value.application, 256);
    number(out, static_cast<quint32>(value.parents.size()), 1);
    for (const auto &parent : value.parents) string(out, parent, 1024);
    return out;
}
PromptMetadata decodePromptMetadata(QByteArray &frame) {
    auto scrub = qScopeGuard([&] { wipe(frame); });
    if (frame.size() < 23 || frame.size() > MaxPromptMetadataBytes || !frame.startsWith("QMP2")) invalid();
    Reader in{QByteArrayView(frame).sliced(4)};
    PromptMetadata result;
    result.processId = in.number(4); result.userId = in.number(4);
    result.label = in.string(1024); result.caller = in.string(256);
    result.executable = in.string(4096); result.operation = in.string(32);
    result.application = in.string(256);
    const auto parents = in.number(1);
    if (parents > 6) invalid();
    for (quint32 i = 0; i < parents; ++i) result.parents.append(in.string(1024));
    if (!in.bytes.isEmpty()) invalid();
    return result;
}
QString promptDisplayText(const QString &value) {
    QString result;
    for (const auto ch : value) {
        const auto category = ch.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format
            || category == QChar::Separator_Line || category == QChar::Separator_Paragraph)
            result += QStringLiteral("\\u%1").arg(static_cast<unsigned>(ch.unicode()), 4, 16, QLatin1Char('0'));
        else result += ch;
    }
    return result;
}
}
