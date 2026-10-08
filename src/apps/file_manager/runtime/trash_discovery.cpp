// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_discovery.h"
#include "../mutation/volume_trash.h"
#include <QDir>
#include <QPointer>

namespace QindaQt::Apps::FileManager {
TrashDiscovery::TrashDiscovery(QObject *parent, Probe probe)
    : QObject(parent), m_probe(probe ? std::move(probe) : Probe(VolumeTrash::discover)),
      m_context(new QObject),
      m_generation(std::make_shared<std::atomic<quint64>>(0)) {
  m_context->moveToThread(&m_thread);
  connect(&m_thread, &QThread::finished, m_context, &QObject::deleteLater);
  m_thread.start();
}
TrashDiscovery::~TrashDiscovery() {
  m_generation->store(0, std::memory_order_relaxed);
  m_thread.quit();
  m_thread.wait();
}
void TrashDiscovery::request(quint64 generation, QStringList roots) {
  m_generation->store(generation, std::memory_order_relaxed);
  roots.removeDuplicates();
  m_pendingDiagnostic.clear();
  if (roots.size() > maximumRoots) {
    roots.clear();
    m_pendingDiagnostic = QStringLiteral("Volume Trash discovery exceeds its bounded root count; unavailable stores were not probed.");
  }
  m_pendingGeneration = generation;
  m_pendingRoots = std::move(roots);
  if (!m_running) dispatch();
}
void TrashDiscovery::dispatch() {
  m_running = true;
  const auto generation = m_pendingGeneration;
  const auto roots = m_pendingRoots;
  const auto initialDiagnostic = m_pendingDiagnostic;
  const auto latest = m_generation;
  const auto probe = m_probe;
  const QPointer<TrashDiscovery> guard(this);
  QMetaObject::invokeMethod(m_context, [guard, latest, generation, roots, initialDiagnostic, probe] {
    QVariantMap paths;
    QString diagnostic = initialDiagnostic;
    for (const auto &root : roots) {
      if (latest->load(std::memory_order_relaxed) != generation) {
        paths.clear(); break;
      }
      if (!root.startsWith(QLatin1Char('/')) || root.contains(QChar::Null) ||
          QDir::cleanPath(root) != root) {
        diagnostic = QStringLiteral("A public device root is not a canonical local path; its Trash was not probed.");
        continue;
      }
      paths.insert(root, probe(root));
    }
    if (!guard) return;
    QMetaObject::invokeMethod(guard, [guard, generation, paths, diagnostic] {
      if (!guard) return;
      guard->m_running = false;
      if (guard->m_pendingGeneration != generation) {
        guard->dispatch(); return;
      }
      emit guard->completed(generation, paths, diagnostic);
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}
}
