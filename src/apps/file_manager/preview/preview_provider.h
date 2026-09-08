// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "local_preview.h"
#include <QCache>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <vector>
namespace QindaQt::Apps::FileManager {
// Engine-owned adapter. GUI composition supplies listing generations; requests
// run in its private two-worker pool. Destruction cancels and joins before the
// injected decoder/cache die. No installed API or compatibility commitment.
class PreviewProvider final : public QQuickAsyncImageProvider {
public:
  explicit PreviewProvider(std::unique_ptr<PreviewDecoder> decoder);
  ~PreviewProvider() override;
  void setGeneration(quint64 generation);
  QQuickImageResponse *requestImageResponse(const QString &id,
                                            const QSize &size) override;
  static constexpr int cacheBytes = 64 * 1024 * 1024;

private:
  std::unique_ptr<PreviewDecoder> m_decoder;
  std::atomic<quint64> m_generation{0};
  QMutex m_mutex;
  QCache<QString, QImage> m_cache{cacheBytes};
  std::vector<std::weak_ptr<std::atomic_bool>> m_requests;
  QThreadPool m_pool;
};
} // namespace QindaQt::Apps::FileManager
