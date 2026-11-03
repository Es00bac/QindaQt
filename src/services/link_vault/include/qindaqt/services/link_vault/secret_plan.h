// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::Link {

// The validated steps of one `vault.type` request (protocol page, "Secret typing").
//
// Ownership: a plan owns keysym buffers that may hold a secret; wipe() (also run by the
// destructor) zeroes them. Move-only so there is never a second copy to forget.
// AGENT-GUARD: nothing here logs, formats or stores step contents. Text is converted to
// keysyms on parse and only the keysym buffer is kept; the QString inside the incoming
// JSON body cannot be zeroed (Qt shares and reallocates it), which the ADR records.
class SecretPlan {
public:
    static constexpr int kMaxSteps = 16;
    static constexpr int kMaxTextBytes = 1024;
    static constexpr int kMaxDelayMs = 2000;
    enum class Kind { Text, Key, Delay };
    struct Step {
        Kind kind = Kind::Text;
        QList<qint32> symbols; // Text: one keysym per typed character
        qint32 keysym = 0;     // Key
        int delayMs = 0;       // Delay
    };

    SecretPlan() = default;
    SecretPlan(const SecretPlan &) = delete;
    SecretPlan &operator=(const SecretPlan &) = delete;
    SecretPlan(SecretPlan &&other) noexcept : m_steps(std::move(other.m_steps)) { other.m_steps.clear(); }
    SecretPlan &operator=(SecretPlan &&other) noexcept
    {
        if (this != &other) {
            wipe();
            m_steps = std::move(other.m_steps);
            other.m_steps.clear();
        }
        return *this;
    }
    ~SecretPlan() { wipe(); }

    // Parses `{token, steps:[{text}|{key}|{delayMs}]}`. nullopt for any wrong type, unknown field,
    // bound violation, unknown key name, or a plan with nothing to type. `*token` receives the token.
    // Characters below U+0020, DEL and C1 are skipped in text (a secret never presses Tab or Return;
    // only a {key} step does). A text that is empty after skipping is dropped.
    [[nodiscard]] static std::optional<SecretPlan> parse(const QJsonObject &body, QString *token);

    [[nodiscard]] qsizetype size() const { return m_steps.size(); }
    // Moves step `i`'s symbols out (text steps), leaving it empty.
    [[nodiscard]] QList<qint32> takeSymbols(qsizetype i) { return std::exchange(m_steps[i].symbols, {}); }
    [[nodiscard]] const Step &at(qsizetype i) const { return m_steps.at(i); }
    void wipe();

private:
    QList<Step> m_steps;
};

} // namespace QindaQt::Link
