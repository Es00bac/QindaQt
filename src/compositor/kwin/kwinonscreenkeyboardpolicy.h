// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

// Applies the `input.touch.onScreenKeyboard` preference to KWin's input
// method (ADR-0204): `off` stops the keyboard; `auto` (anything else)
// leaves KWin's own rule, a finger or pen focused the field.
class KWinOnScreenKeyboardPolicy final : public QObject {
    Q_OBJECT

public:
    explicit KWinOnScreenKeyboardPolicy(QObject *parent = nullptr);

    void apply(const QString &mode);
    [[nodiscard]] const QString &mode() const noexcept { return m_mode; }

private:
    QString m_mode = QStringLiteral("auto");
};

} // namespace QindaQt::Compositor::KWinIntegration
