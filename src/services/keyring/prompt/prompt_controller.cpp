// SPDX-License-Identifier: GPL-3.0-or-later
#include "prompt_controller.h"
#include "../daemon/wire_types.h"
#include <QCoreApplication>
#include <openssl/crypto.h>
#include <unistd.h>
#include <cerrno>

namespace qindaqt::keyring::service {
bool PromptController::approve(QString password, QString confirmation) {
    auto bytes = password.toUtf8(), repeated = confirmation.toUtf8();
    const bool valid = !bytes.isEmpty() && bytes.size() <= 4096 && (!creation ||
        (bytes.size() == repeated.size() && CRYPTO_memcmp(bytes.constData(),repeated.constData(),
                                                       static_cast<std::size_t>(bytes.size())) == 0));
    if (!password.isEmpty()) OPENSSL_cleanse(password.data(),static_cast<std::size_t>(password.size()) * sizeof(QChar));
    if (!confirmation.isEmpty()) OPENSSL_cleanse(confirmation.data(),static_cast<std::size_t>(confirmation.size()) * sizeof(QChar));
    Q_EMIT clearFields();
    bool written = valid;
    qsizetype offset = 0;
    while (written && offset < bytes.size()) {
        const auto n = write(STDOUT_FILENO,bytes.constData() + offset,static_cast<std::size_t>(bytes.size() - offset));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) written = false; else offset += n;
    }
    wipe(bytes); wipe(repeated);
    if (written) QCoreApplication::exit(0);
    else { status = tr("Enter a nonempty password; confirmation must match."); Q_EMIT statusChanged(); }
    return written;
}
void PromptController::cancel() { Q_EMIT clearFields(); QCoreApplication::exit(1); }
}
