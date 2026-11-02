// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/input_frames.h>
#include <qindaqt/services/link_input/text_typer.h>

namespace QindaQt::Link {

TextTyper::TextTyper(InputSink &sink, QObject *parent) : QObject(parent), m_sink(sink)
{
    m_timer.setInterval(4);
    connect(&m_timer, &QTimer::timeout, this, &TextTyper::pump);
}

bool TextTyper::enqueue(const QString &token, const QString &text)
{
    Job job;
    job.token = token;
    const QList<uint> cps = text.toUcs4();
    for (uint cp : cps) {
        if (cp == '\r') continue;
        if (cp == '\n') job.symbols.append(0xff0d);       // Return
        else if (cp == '\t') job.symbols.append(0xff09);  // Tab
        else if (cp >= 0x20 && cp != 0x7f && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff))
            job.symbols.append(InputFrames::keysymForCodepoint(cp));
    }
    if (job.symbols.isEmpty() || m_queued + job.symbols.size() > kMaxQueued) return false;
    m_queued += job.symbols.size();
    m_done.remove(token);
    m_jobs.push_back(std::move(job));
    if (!m_timer.isActive()) m_timer.start();
    return true;
}

TextTyper::State TextTyper::state(const QString &token) const
{
    for (const Job &j : m_jobs) {
        if (j.token == token) return State::Pending;
    }
    return m_done.value(token, State::Unknown);
}

void TextTyper::record(const QString &token, State state)
{
    m_done.insert(token, state);
    m_doneOrder.append(token);
    while (m_doneOrder.size() > 32) m_done.remove(m_doneOrder.takeFirst());
}

void TextTyper::cancel(const QString &prefix)
{
    for (auto it = m_jobs.begin(); it != m_jobs.end();) {
        if (prefix.isEmpty() || it->token.startsWith(prefix)) {
            m_queued -= it->symbols.size() - it->next;
            record(it->token, State::Failed);
            it = m_jobs.erase(it);
        } else {
            ++it;
        }
    }
    if (m_jobs.empty()) m_timer.stop();
}

void TextTyper::pump()
{
    int budget = kBatch;
    while (budget > 0 && !m_jobs.empty()) {
        Job &job = m_jobs.front();
        bool ok = true;
        while (budget > 0 && job.next < job.symbols.size()) {
            const qint32 sym = job.symbols[job.next++];
            --budget;
            --m_queued;
            if (!m_sink.keysym(sym, true) || !m_sink.keysym(sym, false)) {
                ok = false;
                break;
            }
        }
        if (!ok || job.next >= job.symbols.size()) {
            const QString token = job.token;
            m_queued -= job.symbols.size() - job.next;
            m_jobs.pop_front();
            record(token, ok ? State::Complete : State::Failed);
            Q_EMIT finished(token, ok);
        }
    }
    if (m_jobs.empty()) m_timer.stop();
}

} // namespace QindaQt::Link
