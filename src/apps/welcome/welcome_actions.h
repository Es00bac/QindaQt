// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Apps::Welcome {

// Maps tutorial actions onto a fixed set of first-party executables. The
// allowlist is intentionally smaller than a general process-launch service.
class WelcomeActions final : public QObject {
    Q_OBJECT
public:
    explicit WelcomeActions(QObject *parent = nullptr);
    Q_INVOKABLE [[nodiscard]] bool launch(const QString &action) const;

private:
    [[nodiscard]] static QString resolveSibling(const QString &executable);
};

} // namespace QindaQt::Apps::Welcome
