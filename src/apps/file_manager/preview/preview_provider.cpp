// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview_provider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QQuickTextureFactory>
#include <algorithm>
#include <limits>
namespace QindaQt::Apps::FileManager {
namespace {
class Response final : public QQuickImageResponse {
public:
  std::shared_ptr<std::atomic_bool> cancelled =
      std::make_shared<std::atomic_bool>(false);
  QImage image;
  void cancel() override { *cancelled = true; }
  QQuickTextureFactory *textureFactory() const override {
    return QQuickTextureFactory::textureFactoryForImage(image);
  }
  void complete(QImage value) {
    if (value.isNull()) {
      value = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
      value.fill(Qt::transparent);
    }
    image = std::move(value);
    emit finished();
  }
};
} // namespace
PreviewProvider::PreviewProvider(std::unique_ptr<PreviewDecoder> decoder)
    : m_decoder(std::move(decoder)) {
  Q_ASSERT(m_decoder);
  m_pool.setMaxThreadCount(2);
}
PreviewProvider::~PreviewProvider() {
  setGeneration(std::numeric_limits<quint64>::max());
  m_pool.waitForDone();
}
void PreviewProvider::setGeneration(quint64 generation) {
  if (m_generation.exchange(generation) == generation)
    return;
  QMutexLocker lock(&m_mutex);
  for (const auto &weak : m_requests)
    if (auto token = weak.lock())
      *token = true;
  m_requests.clear();
}
QQuickImageResponse *PreviewProvider::requestImageResponse(const QString &id,
                                                           const QSize &) {
  auto *response = new Response;
  {
    QMutexLocker lock(&m_mutex);
    std::erase_if(m_requests,
                  [](const auto &token) { return token.expired(); });
    m_requests.push_back(response->cancelled);
  }
  const int separator = static_cast<int>(id.indexOf(QLatin1Char('/')));
  const quint64 generation = id.left(separator).toULongLong();
  const QString key = id.mid(separator + 1);
  m_pool.start([this, response, generation, key] {
    if (*response->cancelled || generation != m_generation) {
      response->complete({});
      return;
    }
    const auto value = QJsonDocument::fromJson(
                           QByteArray::fromBase64(
                               key.toLatin1(), QByteArray::Base64UrlEncoding))
                           .object();
    DirectoryEntry entry;
    entry.absolutePath = value["path"].toString();
    entry.device = value["device"].toString().toULongLong();
    entry.inode = value["inode"].toString().toULongLong();
    entry.identitySize = value["size"].toString().toLongLong();
    entry.modifiedNanoseconds = value["mtime"].toString().toLongLong();
    entry.mode = value["mode"].toString().toUInt();
    if (!previewIdentityMatches(entry)) {
      response->complete({});
      return;
    }
    QImage image;
    {
      QMutexLocker lock(&m_mutex);
      if (const auto *cached = m_cache.object(key))
        image = *cached;
    }
    if (image.isNull()) {
      image = m_decoder->decode(entry, *response->cancelled);
      if (!image.isNull() && !*response->cancelled &&
          generation == m_generation) {
        QMutexLocker lock(&m_mutex);
        m_cache.insert(key, new QImage(image),
                       static_cast<int>(image.sizeInBytes()));
      }
    }
    // AGENT-GUARD: A completed old folder decode must never republish into a
    // new listing. Identity/revision is also part of the cache key and decoder.
    response->complete(*response->cancelled || generation != m_generation ||
                               !previewIdentityMatches(entry)
                           ? QImage{}
                           : image);
  });
  return response;
}
} // namespace QindaQt::Apps::FileManager
