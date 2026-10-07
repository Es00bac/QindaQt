// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_text.h"
#include <QImage>
#include <QMetaType>
#include <QString>
#include <atomic>
#include <memory>

namespace Poppler { class Document; }

namespace QindaQt::Viewer {

struct RenderRequest {
    quint64 revision = 0;
    QString path;
    QByteArray password = {};
    int page = 0;
    int rotation = 0;
    double scale = 1.0;
};

struct RenderResult {
    quint64 revision = 0;
    QString error;
    bool locked = false;
    bool pdf = false;
    bool textAllowed = false;
    QString pageText;
    QString textError;
    int pageCount = 0;
    int page = 0;
    QSizeF pageSize;
    QImage image;
};

// Worker-thread-only decoder. Owns the current document and never shares Poppler
// objects with the GUI. Results own implicitly shared pixels. A newer revision
// cancels PDF rendering and makes obsolete queued requests no-ops.
class DocumentRenderer {
public:
    explicit DocumentRenderer(std::shared_ptr<std::atomic<quint64>> latest);
    ~DocumentRenderer();
    void clear();
    RenderResult render(const RenderRequest &request);
    TextSearchResult find(const TextSearchRequest &request,
        const std::shared_ptr<std::atomic<quint64>> &latestSearch);
    static QSize boundedSize(QSizeF naturalSize, double scale);
    static constexpr qint64 MaxPixels = 16 * 1024 * 1024;
    static constexpr int MaxDimension = 8192;

private:
    QString load(const RenderRequest &request);
    PageText pageText(int page);
    std::shared_ptr<std::atomic<quint64>> m_latest;
    QString m_path;
    QByteArray m_password;
    std::unique_ptr<Poppler::Document> m_pdf;
    QImage m_image;
    QSizeF m_imageSize;
    bool m_svg = false;
};

} // namespace QindaQt::Viewer
Q_DECLARE_METATYPE(QindaQt::Viewer::RenderResult)
