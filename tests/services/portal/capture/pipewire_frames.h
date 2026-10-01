// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QImage>
#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <pipewire/pipewire.h>
#include <spa/param/video/raw.h>
// Test-owned consumer uses only the actual frontend's returned remote FD.
// Qt-thread loop, bounded mapped raw frames; no daemon/pathname connection.
class PipeWireFrames final : public QObject {
public:
    PipeWireFrames(int remoteFd, quint32 offeredNode);
    ~PipeWireFrames() override;
    bool valid() const { return m_stream && m_error.isEmpty(); }
    int count() const { return m_frames; }
    const QImage &image() const { return m_image; }
    const QSet<quint64> &checksums() const { return m_checksums; }
    const QSet<quint32> &nodes() const { return m_nodes; }
    QString error() const { return m_error; }
private:
    static void process(void *);
    pw_loop *m_loop = nullptr; pw_context *m_context = nullptr; pw_core *m_core = nullptr;
    pw_stream *m_stream = nullptr; pw_registry *m_registry = nullptr;
    spa_hook m_listener{}, m_registryListener{}, m_coreListener{}; spa_video_info_raw m_format{};
    QTimer m_poll; int m_frames = 0; QImage m_image; QSet<quint64> m_checksums;
    QSet<quint32> m_nodes; QString m_error;
    quint32 m_targetNode; QByteArray m_targetSerial;
    int m_registrySync = -1; bool m_registryReady = false;
};
