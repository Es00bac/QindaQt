// SPDX-License-Identifier: GPL-3.0-or-later
#include "prompt_controller.h"
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <qindaqt/services/keyring/secure_buffer.h>
#include <algorithm>
#include <QCoreApplication>
#include <openssl/crypto.h>
#include <unistd.h>
#include <cerrno>

namespace qindaqt::keyring::service {
bool PromptController::approve(QString password, QString confirmation, QString oldPassword) {
    auto bytes = password.toUtf8(), repeated = confirmation.toUtf8(), old = oldPassword.toUtf8();
    const bool valid = confirming || (!bytes.isEmpty() && bytes.size() <= 4096 && (!(creation || changing) ||
        (bytes.size() == repeated.size() && CRYPTO_memcmp(bytes.constData(),repeated.constData(),
                                                       static_cast<std::size_t>(bytes.size())) == 0))
        && (!changing || (!old.isEmpty() && old.size() <= 4096)));
    if (!password.isEmpty()) OPENSSL_cleanse(password.data(),static_cast<std::size_t>(password.size()) * sizeof(QChar));
    if (!confirmation.isEmpty()) OPENSSL_cleanse(confirmation.data(),static_cast<std::size_t>(confirmation.size()) * sizeof(QChar));
    if (!oldPassword.isEmpty()) OPENSSL_cleanse(oldPassword.data(),static_cast<std::size_t>(oldPassword.size()) * sizeof(QChar));
    Q_EMIT clearFields();
    SecureBuffer frame;
    try { frame = SecureBuffer(confirming ? 4 : valid ? static_cast<std::size_t>(bytes.size() + (changing ? old.size()+8 : 0)) : 0); }
    catch (const std::exception &) {
        protocol::wipe(bytes); protocol::wipe(repeated); protocol::wipe(old);
        status = tr("Secure memory is unavailable."); Q_EMIT statusChanged(); return false;
    }
    auto output = frame.bytes();
    if (confirming) {
        output[0]='Q';output[1]='K';output[2]='O';output[3]='K';
    } else if (valid && changing) {
        output[0]='Q';output[1]='K';output[2]='P';output[3]='1';
        output[4]=static_cast<unsigned char>(old.size() >> 8);output[5]=static_cast<unsigned char>(old.size() & 255);
        output[6]=static_cast<unsigned char>(bytes.size() >> 8);output[7]=static_cast<unsigned char>(bytes.size() & 255);
        std::copy(old.begin(),old.end(),output.begin()+8);
        std::copy(bytes.begin(),bytes.end(),output.begin()+8+old.size());
    } else if (valid) std::copy(bytes.begin(),bytes.end(),output.begin());
    protocol::wipe(bytes); protocol::wipe(repeated); protocol::wipe(old);
    bool written = valid;
    std::size_t offset = 0;
    while (written && offset < output.size()) {
        const auto n = write(STDOUT_FILENO,output.data() + offset,output.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) written = false; else offset += static_cast<std::size_t>(n);
    }
    frame.clear();
    if (written) QCoreApplication::exit(0);
    else { status = tr("Enter a nonempty password; confirmation must match."); Q_EMIT statusChanged(); }
    return written;
}
void PromptController::cancel() { Q_EMIT clearFields(); QCoreApplication::exit(1); }
}
