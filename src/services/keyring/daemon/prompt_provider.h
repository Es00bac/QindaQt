// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/secure_buffer.h>
#include <QObject>
#include <QString>
#include <functional>
namespace qindaqt::keyring::service {
struct PromptRequest { QString action, collection, label, caller, window; };
using PromptCompletion = std::function<void(SecureBuffer, bool)>;
// GUI-independent asynchronous seam. Completion must never run inline from
// begin; exactly once or cancelled. IDs/caller contain no password. Caller
// owns receiver lifetime and stale-generation fencing. No secret argv/env/files.
class PromptProvider : public QObject {
public:
    using QObject::QObject;
    virtual quint64 begin(PromptRequest, PromptCompletion) = 0;
    virtual void cancel(quint64) = 0;
};
}
