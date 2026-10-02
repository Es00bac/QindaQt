// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_types.h>
#include <qindaqt/services/portal/request_registry.h>
#include <qindaqt/services/compositor_capture/kwin_capture_port.h>
#include <qindaqt/services/compositor_capture/wayland_screencast.h>
#include "native_capture_admission.h"
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QSet>
namespace QindaQt::Services::Portal {
// Native presentation owns no actor/permission persistence. Borrowed capture
// ports and admission outlive the dialog; all operation callbacks recheck it.
class CaptureDialog final : public QDialog {
    Q_OBJECT
public:
    CaptureDialog(CaptureRequest, QString directory, NativeCaptureAdmission &,
        CompositorCapture::KWinCapturePort &, CompositorCapture::WaylandScreenCast &);
    void parentReady();
    void captureReady();
    void fail();
Q_SIGNALS:
    void result(RequestResponse, const QJsonObject &);
    void consented();
private:
    void refresh();
    void begin();
    void image(const CompositorCapture::DecodedCapture &);
    void finish(RequestResponse, const QJsonObject & = {});
    CaptureRequest m_request; QString m_directory;
    NativeCaptureAdmission &m_admission; CompositorCapture::KWinCapturePort &m_capture;
    CompositorCapture::WaylandScreenCast &m_stream;
    QVBoxLayout *m_layout; QListWidget *m_sources; QPushButton *m_allow, *m_cancel;
    QStringList m_selectedSources; QSet<quint32> m_nodes; QJsonArray m_streamResults;
    bool m_parent = false, m_busy = false, m_sent = false, m_finished = false, m_granted = false;
};
}
