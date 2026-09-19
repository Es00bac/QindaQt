// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewer_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPointer>
#include <algorithm>
#include <cmath>

namespace QindaQt::Viewer {
ViewerController::ViewerController(QObject *parent)
    : QObject(parent), m_latest(std::make_shared<std::atomic<quint64>>(0)),
      m_renderer(std::make_shared<DocumentRenderer>(m_latest))
{
    QImageReader::setAllocationLimit(256);
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("viewer-renderer"));
    m_thread.start();
}

ViewerController::~ViewerController()
{
    ++(*m_latest);
    QMetaObject::invokeMethod(m_worker, [renderer = m_renderer] { renderer->clear(); },
                              Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

QString ViewerController::fileName() const { return QFileInfo(m_request.path).fileName(); }

QUrl ViewerController::localArgument(const QString &argument)
{
    if (argument.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
        return QUrl(argument);
    const QUrl possibleUrl(argument);
    if (!possibleUrl.scheme().isEmpty())
        return possibleUrl;
    return QUrl::fromLocalFile(QDir::current().absoluteFilePath(argument));
}

void ViewerController::close()
{
    ++(*m_latest);
    QMetaObject::invokeMethod(m_worker, [renderer = m_renderer] { renderer->clear(); });
    m_request = {};
    m_frame = {};
    m_error.clear();
    m_pageSize = {};
    m_pageCount = 0;
    m_page = 0;
    m_busy = false;
    m_locked = false;
    m_opening = false;
    ++m_frameRevision;
    emit frameChanged();
    emit stateChanged();
}

void ViewerController::open(const QUrl &url)
{
    close();
    if (!url.isValid() || !url.isLocalFile() || !url.host().isEmpty()
        || url.hasQuery() || url.hasFragment() || url.toLocalFile().isEmpty()) {
        m_error = tr("Open a local image or PDF file. Remote URLs are not supported.");
        emit stateChanged();
        return;
    }
    m_request.path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    m_opening = true;
    requestRender();
}

void ViewerController::unlock(const QString &password)
{
    if (!m_locked)
        return;
    // AGENT-CONTRACT: Poppler Qt6's Document::load password arrays are Latin-1,
    // not UTF-8; the latter prevents valid non-ASCII legacy PDFs from opening.
    m_request.password = password.toLatin1();
    m_opening = true;
    requestRender();
}

void ViewerController::goToPage(int page)
{
    if (!ready() || page < 0 || page >= m_pageCount || page == m_page)
        return;
    m_page = page;
    m_request.page = page;
    requestRender();
}

void ViewerController::rotate(int degrees)
{
    if (!ready() || degrees % 90 != 0)
        return;
    m_request.rotation = (m_request.rotation + degrees % 360 + 360) % 360;
    requestRender();
}

void ViewerController::renderAt(double zoom, double devicePixelRatio)
{
    if (!ready() || !std::isfinite(zoom) || !std::isfinite(devicePixelRatio)
        || zoom <= 0 || devicePixelRatio <= 0)
        return;
    const double scale = std::clamp(zoom, 0.05, 8.0)
        * std::clamp(devicePixelRatio, 1.0, 4.0);
    if (std::abs(m_request.scale - scale) < 0.001)
        return;
    m_request.scale = scale;
    requestRender();
}

void ViewerController::requestRender()
{
    m_request.revision = ++(*m_latest);
    m_busy = true;
    m_error.clear();
    emit stateChanged();
    const auto request = m_request;
    const auto renderer = m_renderer;
    // AGENT-GUARD: only value snapshots cross the thread boundary. Queued
    // results are revision-fenced after close, another open, or navigation.
    QMetaObject::invokeMethod(m_worker, [this, renderer, request] {
        RenderResult result = renderer->render(request);
        QMetaObject::invokeMethod(this, [this, result = std::move(result)]() mutable {
            acceptResult(std::move(result));
        });
    });
}

void ViewerController::acceptResult(RenderResult result)
{
    if (result.revision != m_latest->load())
        return;
    m_busy = false;
    m_error = result.error;
    m_locked = result.locked;
    m_pageCount = result.pageCount;
    m_page = result.page;
    m_pageSize = result.pageSize;
    m_frame = std::move(result.image);
    ++m_frameRevision;
    emit frameChanged();
    emit stateChanged();
    if (m_locked) {
        m_error = m_request.password.isEmpty() ? tr("This PDF needs a password.")
                                              : tr("The password did not unlock this PDF.");
        emit stateChanged();
        emit passwordRequired();
    } else if (m_opening && ready()) {
        m_opening = false;
        emit opened();
    }
}
} // namespace QindaQt::Viewer
