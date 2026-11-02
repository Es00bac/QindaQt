// SPDX-License-Identifier: GPL-3.0-or-later
#include "prompt_controller.h"
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <qindaqt/services/keyring/secure_buffer.h>
#include <algorithm>
#include <QCoreApplication>
#include <QFileInfo>
#include <openssl/crypto.h>
#include <unistd.h>
#include <cerrno>

namespace qindaqt::keyring::service {
bool PromptController::setRequest(const QString &action, const protocol::PromptMetadata &value) {
    auto expected = value.operation;
    if (expected == "portal-unlock" || expected == "import-unlock") expected = "unlock";
    if (expected == "portal-create" || expected == "import-create") expected = "create";
    if (action != expected || (action != "unlock" && action != "create" && action != "reveal"
        && action != "change-password" && action != "confirm-delete")) return false;
    creation = action == "create"; changing = action == "change-password";
    revealing = action == "reveal"; confirming = action == "confirm-delete";
    label = protocol::promptDisplayText(value.label);
    application = protocol::promptDisplayText(value.application);
    requesterKnown = value.processId > 0;
    executableKnown = value.executable.startsWith('/');
    requester = requesterKnown
        ? (executableKnown
            ? tr("%1 (PID %2)").arg(protocol::promptDisplayText(QFileInfo(value.executable).fileName())).arg(value.processId)
            : tr("Executable identity unavailable (PID %1)").arg(value.processId))
        : tr("Requester identity unavailable — approval is disabled");
    requesterDetails = tr("Executable: %1\nUser ID: %2 · Connection: %3")
        .arg(value.executable.isEmpty() ? tr("Unavailable") : protocol::promptDisplayText(value.executable))
        .arg(requesterKnown ? QString::number(value.userId) : tr("Unavailable")).arg(protocol::promptDisplayText(value.caller));
    QStringList parents;
    for (const auto &parent : value.parents) parents.append(protocol::promptDisplayText(parent));
    parentContext = parents.isEmpty() ? tr("Parent process context unavailable.") : parents.join('\n');
    if (value.operation.startsWith("import-"))
        operation = tr("The keyring importer needs this password to import saved credentials into the native keyring.");
    else if (value.operation.startsWith("portal-"))
        operation = tr("The desktop portal requested an application secret for the application shown below.");
    else if (creation) operation = tr("This program asked to create a keyring for saved credentials.");
    else if (changing) operation = tr("This program asked to change this keyring's password.");
    else if (revealing) operation = tr("This program asked to read a saved secret using fresh authentication.");
    else if (confirming) operation = tr("This program asked to permanently delete the saved item shown below.");
    else operation = tr("This program asked to unlock a keyring so it can access saved credentials.");
    if (confirming) scope = tr("Approval permanently deletes this saved item.");
    else if (value.operation.startsWith("import-"))
        scope = tr("Approval allows the importer to decrypt or create this collection and write imported credentials to it.");
    else {
        scope = tr("The unlocked keyring is available to other programs running as you. This is not permission for just this program or agent task.");
        if (value.operation == "unlock") scope += tr(" Other keyrings using the same password may also unlock.");
        if (revealing) scope += tr(" The requested secret will be returned to this program.");
        if (changing) scope += tr(" The new password replaces the current keyring password.");
    }
    return true;
}
bool PromptController::review() {
    if (!requesterKnown) return false;
    reviewed_ = true; Q_EMIT reviewChanged(); return true;
}
bool PromptController::approve(QString password, QString confirmation, QString oldPassword) {
    auto bytes = password.toUtf8(), repeated = confirmation.toUtf8(), old = oldPassword.toUtf8();
    const bool valid = reviewed_ && requesterKnown && (confirming || (!bytes.isEmpty() && bytes.size() <= 4096 && (!(creation || changing) ||
        (bytes.size() == repeated.size() && CRYPTO_memcmp(bytes.constData(),repeated.constData(),
                                                       static_cast<std::size_t>(bytes.size())) == 0))
        && (!changing || (!old.isEmpty() && old.size() <= 4096))));
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
    else { status = !reviewed_ ? tr("Review the requester and access scope before continuing.")
                              : tr("Enter a nonempty password; confirmation must match."); Q_EMIT statusChanged(); }
    return written;
}
void PromptController::cancel() { Q_EMIT clearFields(); QCoreApplication::exit(1); }
}
