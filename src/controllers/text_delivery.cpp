// SPDX-License-Identifier: GPL-3.0-or-later
#include "text_delivery_p.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QElapsedTimer>

namespace QindaQt::Controllers {
TextDelivery::TextDelivery(TextOutput &output, QObject *parent) : QObject(parent), m_output(output) {
    m_timer.setInterval(4);
    connect(&m_timer, &QTimer::timeout, this, &TextDelivery::step);
}
void TextDelivery::start(const QString &token, const QString &text) {
    cancel();
    m_finishedToken = token; m_success = false;
    if (!m_output.allowed() || !m_output.target() || text.isEmpty() || !text.isValidUtf16() || text.toUtf8().size() > MaxTextBytes) { Q_EMIT failed(); return; }
    m_target = m_output.target(); m_text = text.toUcs4(); m_token = token; m_offset = 0;
    m_timer.start();
}
void TextDelivery::cancel(const QString &prefix) {
    if (!prefix.isEmpty() && !m_token.startsWith(prefix + '/')) return;
    if (!m_token.isEmpty()) finish(false);
}
void TextDelivery::finish(bool success) {
    m_finishedToken = m_token; m_success = success;
    m_timer.stop(); m_target.clear(); m_text.clear(); m_token.clear(); m_offset = 0;
}
void TextDelivery::step() {
    QElapsedTimer budget; budget.start();
    // AGENT-GUARD: text cannot migrate to a different window, survive a page
    // switch, or block a compositor frame for an entire Unicode text block.
    for (int chunks = 0; chunks < 16 && m_offset < m_text.size(); ++chunks) {
        if (!m_target || m_target != m_output.target() || !m_output.allowed()) { cancel(); return; }
        const auto n = std::min<qsizetype>(std::clamp(m_output.chunkCodepoints(), 1, 128), m_text.size() - m_offset);
        const auto text = QString::fromUcs4(reinterpret_cast<const char32_t *>(m_text.constData() + m_offset), n);
        if (!m_output.insert(text)) { cancel(); Q_EMIT failed(); return; }
        m_offset += n;
        if (budget.nsecsElapsed() >= 2'000'000) break;
    }
    if (m_offset >= m_text.size()) finish(true);
}
} // namespace QindaQt::Controllers
