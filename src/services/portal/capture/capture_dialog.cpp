// SPDX-License-Identifier: GPL-3.0-or-later
#include "capture_dialog.h"
#include "color_picker.h"
#include <QDir>
#include <QLabel>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QJsonArray>
namespace QindaQt::Services::Portal {
CaptureDialog::CaptureDialog(CaptureRequest request, QString directory, NativeCaptureAdmission &admission,
    CompositorCapture::KWinCapturePort &capture, CompositorCapture::WaylandScreenCast &stream)
    : m_request(std::move(request)), m_directory(std::move(directory)), m_admission(admission), m_capture(capture), m_stream(stream) {
    setWindowTitle(m_request.kind == CaptureKind::Stream ? tr("Share a screen") : tr("Screen capture permission"));
    setObjectName("nativeCaptureDialog"); setModal(m_request.modal); resize(580, 360);
    m_layout = new QVBoxLayout(this); auto *question = new QLabel(this); question->setTextFormat(Qt::PlainText); question->setWordWrap(true);
    const QString app = m_request.app.isEmpty() ? tr("An application") : m_request.app;
    question->setText(m_request.kind == CaptureKind::Stream ? tr("%1 wants to share a screen. Choose a screen and allow sharing. You can stop sharing at any time.").arg(app)
        : m_request.kind == CaptureKind::Color ? tr("%1 wants to pick a color from your screen. Allow a capture, then choose a pixel.").arg(app) : tr("%1 wants a screenshot of your screens.").arg(app));
    m_layout->addWidget(question); m_sources = new QListWidget(this); m_sources->setObjectName("captureSources");
    m_sources->setAccessibleName(tr("Screens available to share")); m_sources->setVisible(m_request.kind == CaptureKind::Stream); m_layout->addWidget(m_sources);
    m_allow = new QPushButton(m_request.kind == CaptureKind::Stream ? tr("Share selected screen") : tr("Allow capture"), this); m_allow->setObjectName("captureAllow"); m_allow->setEnabled(false);
    m_cancel = new QPushButton(tr("Cancel"), this); m_cancel->setObjectName("captureCancel"); m_layout->addWidget(m_allow); m_layout->addWidget(m_cancel);
    connect(m_allow, &QPushButton::clicked, this, &CaptureDialog::begin); connect(m_cancel, &QPushButton::clicked, this, [this] { finish(RequestResponse::Cancelled); });
    connect(&m_admission, &NativeCaptureAdmission::ready, this, &CaptureDialog::refresh); connect(&m_admission, &NativeCaptureAdmission::lost, this, &CaptureDialog::fail);
    connect(&m_stream, &CompositorCapture::WaylandScreenCast::sourcesReady, this, [this] {
        m_sources->clear(); for (const auto &source : m_stream.sources()) { auto *item = new QListWidgetItem(source.name, m_sources); item->setData(Qt::UserRole, source.id); } refresh();
    });
    connect(m_sources, &QListWidget::itemSelectionChanged, this, &CaptureDialog::refresh);
    connect(&m_stream, &CompositorCapture::WaylandScreenCast::streamCreated, this, [this](quint32 node, const CompositorCapture::MonitorSource &source) {
        if (!m_parent || !m_admission.admitted() || m_finished || m_sent) { fail(); return; }
        m_sent = true; m_allow->hide(); m_sources->setEnabled(false); m_cancel->setText(tr("Stop sharing"));
        Q_EMIT result(RequestResponse::Success, {{"node", static_cast<double>(node)}, {"name", source.name}, {"x", source.position.x()}, {"y", source.position.y()}, {"width", source.size.width()}, {"height", source.size.height()}});
    });
    connect(&m_stream, &CompositorCapture::WaylandScreenCast::closed, this, [this] { if (m_request.kind == CaptureKind::Stream) fail(); });
    connect(&m_capture, &CompositorCapture::CapturePort::finished, this, &CaptureDialog::image);
    connect(this, &QDialog::rejected, this, [this] { finish(RequestResponse::Cancelled); });
    QTimer::singleShot(4000, this, [this] { if (!m_parent || !m_admission.admitted()) fail(); });
}
void CaptureDialog::parentReady() { m_parent = true; refresh(); }
void CaptureDialog::refresh() { m_allow->setEnabled(m_parent && m_admission.admitted() && !m_busy && (m_request.kind != CaptureKind::Stream || m_sources->currentItem())); }
void CaptureDialog::begin() {
    if (!m_parent || !m_admission.admitted() || m_busy || m_finished) { fail(); return; }
    m_busy = true; refresh();
    if (m_request.kind == CaptureKind::Stream) {
        const auto *selected = m_sources->currentItem(); if (!selected || !m_stream.start(selected->data(Qt::UserRole).toString())) fail(); return;
    }
    hide(); QTimer::singleShot(100, this, [this] {
        if (!m_parent || !m_admission.admitted() || m_finished) { fail(); return; }
        CompositorCapture::CaptureOptions options; options.mode = CompositorCapture::CaptureMode::AllScreens;
        if (!m_capture.capture(CompositorCapture::kwinCallFor(options))) fail();
    });
}
void CaptureDialog::image(const CompositorCapture::DecodedCapture &result) {
    if (m_finished || !m_parent || !m_admission.admitted() || !result.ok()) { fail(); return; }
    if (m_request.kind == CaptureKind::Color) {
        m_allow->hide(); m_sources->hide(); auto *picker = new ColorPicker(result.image, this); m_layout->insertWidget(1, picker);
        connect(picker, &ColorPicker::chosen, this, [this](const QColor &color) { if (m_admission.admitted()) finish(RequestResponse::Success, {{"color", QJsonArray{color.redF(), color.greenF(), color.blueF()}}}); else fail(); });
        show(); picker->setFocus(); return;
    }
    QSaveFile file(QDir(m_directory).filePath("screenshot.png"));
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) || !result.image.save(&file, "PNG") || !m_admission.admitted() || !file.commit()) { fail(); return; }
    finish(RequestResponse::Success, {{"uri", QUrl::fromLocalFile(file.fileName()).toString(QUrl::FullyEncoded)}});
}
void CaptureDialog::finish(RequestResponse response, const QJsonObject &results) {
    if (m_finished) return;
    m_finished = true; m_capture.cancel(); m_stream.stop(); hide();
    if (response == RequestResponse::Success && !m_admission.admitted()) response = RequestResponse::Failed;
    if (!m_sent) { m_sent = true; Q_EMIT result(response, response == RequestResponse::Success ? results : QJsonObject{}); }
    done(response == RequestResponse::Success ? QDialog::Accepted : QDialog::Rejected);
}
void CaptureDialog::fail() { finish(RequestResponse::Failed); }
}
