// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model/file_manager_types.h"
#include <QImage>
#include <atomic>
#include <memory>

namespace QindaQt::Apps::FileManager {
// Private worker-thread seam. Implementations return a null image on any
// refusal, honor cancellation between bounded reads, and never perform GUI or
// disk-cache I/O.
class PreviewDecoder {
public:
  virtual ~PreviewDecoder() = default;
  virtual QImage decode(const DirectoryEntry &entry,
                        const std::atomic_bool &cancelled) const = 0;
};
class LocalPreviewDecoder final : public PreviewDecoder {
public:
  static constexpr qint64 maximumBytes = 32 * 1024 * 1024;
  static constexpr qint64 maximumPixels = 40 * 1000 * 1000;
  // ADR-0111's thumbnail bound, and (ADR-0270) the Gallery view's one large
  // preview. The input limits above are the same for both.
  static constexpr int thumbnailEdge = 192;
  static constexpr int galleryEdge = 1024;
  explicit LocalPreviewDecoder(int maximumEdge = thumbnailEdge);
  QImage decode(const DirectoryEntry &entry,
                const std::atomic_bool &cancelled) const override;

private:
  int m_maximumEdge;
};
bool previewIdentityMatches(const DirectoryEntry &entry);
QString previewUrl(const DirectoryEntry &entry, quint64 generation);
QString entryIconName(const DirectoryEntry &entry);
} // namespace QindaQt::Apps::FileManager
