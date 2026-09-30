// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <QStringDecoder>
#include <stdexcept>
namespace qindaqt::keyring::protocol {
namespace {
[[noreturn]] void invalid() { throw std::runtime_error("Invalid prompt metadata"); }
QString utf8(QByteArrayView bytes) {
    QStringDecoder decode(QStringDecoder::Utf8);
    const QString value=decode(bytes);
    if(decode.hasError() || value.contains(QChar(0))) invalid();
    return value;
}
}
QByteArray encodePromptMetadata(const PromptMetadata &value) {
    auto label=value.label.toUtf8(),caller=value.caller.toUtf8();
    if(label.size()>1024 || caller.size()>256 || utf8(label)!=value.label || utf8(caller)!=value.caller) invalid();
    QByteArray output("QMP1",4);
    output.append(static_cast<char>(label.size()>>8));output.append(static_cast<char>(label.size()&255));
    output.append(static_cast<char>(caller.size()>>8));output.append(static_cast<char>(caller.size()&255));
    output.append(label);output.append(caller);wipe(label);wipe(caller);return output;
}
PromptMetadata decodePromptMetadata(QByteArray &frame) {
    if(frame.size()<8 || frame.size()>1288 || !frame.startsWith("QMP1")) invalid();
    const auto u=[](char c) { return static_cast<unsigned char>(c); };
    const qsizetype label=(qsizetype(u(frame[4]))<<8)|u(frame[5]);
    const qsizetype caller=(qsizetype(u(frame[6]))<<8)|u(frame[7]);
    if(label>1024 || caller>256 || frame.size()!=8+label+caller) invalid();
    PromptMetadata result{utf8(QByteArrayView(frame).sliced(8,label)),
                          utf8(QByteArrayView(frame).sliced(8+label,caller))};
    wipe(frame);return result;
}
}
