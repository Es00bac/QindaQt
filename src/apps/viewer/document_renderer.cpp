// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_renderer.h"

#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QTransform>
#include <QVariant>
#include <poppler-qt6.h>
#include <algorithm>
#include <cmath>

namespace QindaQt::Viewer {
namespace {
struct Cancellation {
    const std::atomic<quint64> *latest;
    quint64 revision;
};
bool shouldAbort(const QVariant &payload)
{
    const auto *cancel = static_cast<const Cancellation *>(payload.value<void *>());
    return cancel->latest->load() != cancel->revision;
}
bool validSize(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height())
        && size.width() >= 1 && size.height() >= 1
        && size.width() <= 1000000 && size.height() <= 1000000;
}
} // namespace

DocumentRenderer::DocumentRenderer(std::shared_ptr<std::atomic<quint64>> latest)
    : m_latest(std::move(latest)) {}
DocumentRenderer::~DocumentRenderer() = default;

void DocumentRenderer::clear()
{
    m_path.clear();
    m_password.clear();
    m_pdf.reset();
    m_image = {};
    m_imageSize = {};
    m_svg = false;
}

QSize DocumentRenderer::boundedSize(QSizeF size, double scale)
{
    if (!validSize(size) || !std::isfinite(scale) || scale <= 0)
        return {};
    const double boundedScale = std::min({scale,
        MaxDimension / size.width(), MaxDimension / size.height(),
        std::sqrt(static_cast<double>(MaxPixels) / (size.width() * size.height()))});
    return {std::max(1, static_cast<int>(std::floor(size.width() * boundedScale))),
            std::max(1, static_cast<int>(std::floor(size.height() * boundedScale)))};
}

QString DocumentRenderer::load(const RenderRequest &request)
{
    if (request.path == m_path && request.password == m_password && (m_pdf || !m_image.isNull()))
        return {};
    clear();
    const QFileInfo info(request.path);
    if (!info.isFile() || !info.isReadable())
        return QStringLiteral("The file does not exist or is not readable.");
    QFile probe(request.path);
    if (!probe.open(QIODevice::ReadOnly))
        return QStringLiteral("The file could not be opened: %1").arg(probe.errorString());
    // Image metadata may itself contain a PDF marker. Prefer a recognized
    // image decoder before accepting a PDF header in the first 1024 bytes.
    const QByteArray imageFormat = QImageReader::imageFormat(request.path);
    const bool isPdf = (imageFormat.isEmpty() || imageFormat == "pdf")
        && probe.read(1024).contains("%PDF-");
    probe.close();
    if (isPdf) {
        m_pdf = Poppler::Document::load(request.path, request.password, request.password);
        if (!m_pdf)
            return QStringLiteral("The PDF is damaged or could not be read.");
        m_pdf->setRenderHint(Poppler::Document::Antialiasing, true);
        m_pdf->setRenderHint(Poppler::Document::TextAntialiasing, true);
    } else {
        QImageReader reader(request.path);
        reader.setAutoTransform(true);
        const QSize rawSize = reader.size();
        if (!validSize(rawSize))
            return QStringLiteral("This is not a supported image or PDF, or its dimensions are invalid.");
        // AGENT-GUARD: formats without native scaling may decode their full
        // source before downsampling. Bound that allocation separately from
        // display pixels; do not disable Qt's image allocation guard.
        const qint64 sourcePixels = static_cast<qint64>(rawSize.width()) * rawSize.height();
        if (sourcePixels > 64 * 1024 * 1024
            && !reader.supportsOption(QImageIOHandler::ScaledSize))
            return QStringLiteral("This image is too large to decode safely.");
        reader.setScaledSize(boundedSize(rawSize, 1.0));
        m_image = reader.read();
        if (m_image.isNull())
            return QStringLiteral("The image could not be decoded: %1").arg(reader.errorString());
        m_imageSize = rawSize;
        m_svg = imageFormat == "svg" || imageFormat == "svgz";
        if ((reader.transformation() & QImageIOHandler::TransformationRotate90) != 0)
            m_imageSize.transpose();
    }
    m_path = request.path;
    m_password = request.password;
    return {};
}

RenderResult DocumentRenderer::render(const RenderRequest &request)
{
    RenderResult result;
    result.revision = request.revision;
    if (m_latest->load() != request.revision)
        return result;
    result.error = load(request);
    if (!result.error.isEmpty() || m_latest->load() != request.revision)
        return result;
    const int quarterTurns = ((request.rotation / 90) % 4 + 4) % 4;
    if (m_pdf) {
        result.locked = m_pdf->isLocked();
        if (result.locked)
            return result;
        result.pageCount = m_pdf->numPages();
        if (result.pageCount <= 0) {
            result.error = QStringLiteral("This PDF has no pages.");
            return result;
        }
        result.page = std::clamp(request.page, 0, result.pageCount - 1);
        const auto page = m_pdf->page(result.page);
        if (!page) {
            result.error = QStringLiteral("This PDF page could not be read.");
            return result;
        }
        // PDF units are points; 100% means 96 logical pixels per inch.
        result.pageSize = page->pageSizeF() * (96.0 / 72.0);
        if (quarterTurns % 2 != 0)
            result.pageSize.transpose();
        const QSize pixels = boundedSize(result.pageSize, request.scale);
        if (pixels.isEmpty()) {
            result.error = QStringLiteral("This PDF page has invalid dimensions.");
            return result;
        }
        const double dpi = 96.0 * std::min(pixels.width() / result.pageSize.width(),
                                         pixels.height() / result.pageSize.height());
        Cancellation cancel{m_latest.get(), request.revision};
        result.image = page->renderToImage(dpi, dpi, -1, -1, -1, -1,
            static_cast<Poppler::Page::Rotation>(quarterTurns), nullptr, nullptr,
            shouldAbort, QVariant::fromValue(static_cast<void *>(&cancel)));
    } else {
        result.pageCount = 1;
        result.pageSize = m_imageSize;
        if (quarterTurns % 2 != 0)
            result.pageSize.transpose();
        QImage source = m_image;
        if (m_svg) {
            QImageReader reader(m_path);
            reader.setScaledSize(boundedSize(m_imageSize, request.scale));
            source = reader.read();
        }
        const QImage rotated = quarterTurns == 0 ? source
            : source.transformed(QTransform().rotate(quarterTurns * 90));
        result.image = rotated.scaled(boundedSize(result.pageSize, request.scale),
                                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (result.image.isNull() && m_latest->load() == request.revision)
        result.error = QStringLiteral("This page could not be rendered.");
    return result;
}
} // namespace QindaQt::Viewer
