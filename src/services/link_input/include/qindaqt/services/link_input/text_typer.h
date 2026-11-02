// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_input/input_sink.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <deque>

namespace QindaQt::Link {

// Types text as keysym press/release pairs in bounded batches, so a long
// string never blocks the event loop and never floods the portal. Shared by
// input.text and macro text steps (and, later, vault typing).
//
// Ownership: borrows the sink, which must outlive it. Threading: GUI thread.
// Error behavior: a sink failure marks that job Failed and drops its rest.
// AGENT-GUARD: text content is never logged, stored beyond delivery, or
// echoed in diagnostics.
class TextTyper final : public QObject {
    Q_OBJECT
public:
    enum class State { Pending, Complete, Failed, Unknown };
    static constexpr int kBatch = 24;          // symbols per timer tick
    static constexpr qsizetype kMaxQueued = 8192; // symbols waiting across jobs

    explicit TextTyper(InputSink &sink, QObject *parent = nullptr);
    // False (nothing queued) when the queue is full or the text is empty.
    bool enqueue(const QString &token, const QString &text);
    [[nodiscard]] State state(const QString &token) const;
    // Drops queued jobs whose token starts with `prefix` (all when empty).
    void cancel(const QString &prefix = {});
    [[nodiscard]] bool busy() const { return !m_jobs.empty(); }

Q_SIGNALS:
    void finished(const QString &token, bool ok);

private:
    struct Job {
        QString token;
        QList<qint32> symbols;
        qsizetype next = 0;
    };
    void pump();
    void record(const QString &token, State state);

    InputSink &m_sink;
    QTimer m_timer;
    std::deque<Job> m_jobs;
    qsizetype m_queued = 0;
    QHash<QString, State> m_done; // bounded memory of recent outcomes
    QStringList m_doneOrder;
};

} // namespace QindaQt::Link
