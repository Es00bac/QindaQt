// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Link {

// Shows a pairing request to the desktop user and reports the decision.
// Production uses a freedesktop notification with Accept/Reject actions;
// tests inject a fake and never talk to the real notification server.
//
// AGENT-CONTRACT: present() may be called again for the same deviceId (a new
// request replaces the old one). dismiss() must be safe for unknown ids.
// decided() is emitted at most once per present().
class PairingPresenter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // `code` is six digits; implementations format it as "123 456".
    virtual void present(const QString &deviceId, const QString &name, const QString &model,
                         const QString &code) = 0;
    virtual void dismiss(const QString &deviceId) = 0;

Q_SIGNALS:
    void decided(const QString &deviceId, bool accepted);
};

// Formats "123456" as "123 456".
[[nodiscard]] inline QString spacedCode(const QString &code)
{
    return code.size() == 6 ? code.left(3) + QLatin1Char(' ') + code.mid(3) : code;
}

} // namespace QindaQt::Link
