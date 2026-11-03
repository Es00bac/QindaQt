// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Link {

// The focused desktop window as the shell reported it (ADR-0440).
// `title` is display text only: it is never compared, parsed or used to
// decide anything. Identity is (uuid, revision).
struct FocusFact {
    QString uuid;
    QString appId;
    QString title;
    quint64 revision = 0;
};

// Where the secret-typing feature learns which window has focus.
//
// Ownership: implementations outlive their borrowers. Threading: GUI thread.
// AGENT-GUARD: nullopt means "unknown" and every consumer must fail closed
// (error{unverified}); never substitute a last-known value.
class FocusSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual std::optional<FocusFact> current() const = 0;
Q_SIGNALS:
    // Emitted when current() changed in identity, revision, app or title.
    void changed();
};

} // namespace QindaQt::Link
