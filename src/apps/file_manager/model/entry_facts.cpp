// SPDX-License-Identifier: GPL-3.0-or-later
#include "entry_facts.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QSize>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <vector>

namespace QindaQt::Apps::FileManager {
namespace {

// A table of names keeps at most this many ids; they are few in practice.
constexpr int maximumCachedNames = 1024;

[[nodiscard]] std::vector<char> nameBuffer(int which) {
  const long size = ::sysconf(which);
  return std::vector<char>(size > 0 ? static_cast<std::size_t>(size) : 16384U);
}

[[nodiscard]] QString userNameFor(qint64 id) {
  std::vector<char> buffer = nameBuffer(_SC_GETPW_R_SIZE_MAX);
  struct passwd entry{};
  struct passwd *found = nullptr;
  if (::getpwuid_r(static_cast<uid_t>(id), &entry, buffer.data(), buffer.size(), &found) == 0 &&
      found != nullptr) {
    return QString::fromLocal8Bit(found->pw_name);
  }
  return QString::number(id);
}

[[nodiscard]] QString groupNameFor(qint64 id) {
  std::vector<char> buffer = nameBuffer(_SC_GETGR_R_SIZE_MAX);
  struct group entry{};
  struct group *found = nullptr;
  if (::getgrgid_r(static_cast<gid_t>(id), &entry, buffer.data(), buffer.size(), &found) == 0 &&
      found != nullptr) {
    return QString::fromLocal8Bit(found->gr_name);
  }
  return QString::number(id);
}

[[nodiscard]] QString countItems(const QString &path, const std::atomic_bool &closing) {
  const QFileInfo folder(path);
  if (!folder.isDir() || !folder.isReadable()) {
    return {};
  }
  QDirIterator children(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                                  QDir::System);
  int count = 0;
  while (children.hasNext() && !closing) {
    children.next();
    if (children.fileName().startsWith(QLatin1Char('.'))) {
      continue;
    }
    if (++count >= EntryFacts::maximumCountedItems) {
      return QLocale().toString(count) + QLatin1Char('+');
    }
  }
  return closing ? QString() : QLocale().toString(count);
}

[[nodiscard]] QString readDimensions(const QString &path) {
  const QFileInfo file(path);
  if (file.isSymLink() || !file.isFile()) {
    return {};
  }
  QImageReader reader(path);
  // Judged by content, as the preview pipeline does, never by the name.
  reader.setDecideFormatFromContent(true);
  // AGENT-GUARD: the preview pipeline's formats only (local_preview.cpp).
  const QByteArray format = reader.format().toLower();
  if (format != "png" && format != "jpeg" && format != "jpg" && format != "bmp" &&
      format != "webp") {
    return {};
  }
  reader.setAutoTransform(true);
  QSize size = reader.size();
  if (!size.isValid()) {
    return {};
  }
  // An EXIF rotation swaps what the user sees.
  if (reader.transformation().testFlag(QImageIOHandler::TransformationRotate90)) {
    size.transpose();
  }
  return QStringLiteral("%1 × %2").arg(size.width()).arg(size.height());
}

} // namespace

EntryFacts::EntryFacts(QObject *parent) : QObject(parent) {
  m_pool.setMaxThreadCount(1);
  m_publish.setSingleShot(true);
  m_publish.setInterval(50);
  connect(&m_publish, &QTimer::timeout, this, [this] {
    ++m_revision;
    emit revisionChanged();
  });
}

EntryFacts::~EntryFacts() {
  *m_closing = true;
  m_pool.clear();
  m_pool.waitForDone();
}

QString EntryFacts::ownerName(qint64 id) {
  if (id < 0) {
    return {};
  }
  if (const auto found = m_owners.constFind(id); found != m_owners.cend()) {
    return *found;
  }
  if (m_owners.size() >= maximumCachedNames) {
    m_owners.clear();
  }
  return *m_owners.insert(id, userNameFor(id));
}

QString EntryFacts::groupName(qint64 id) {
  if (id < 0) {
    return {};
  }
  if (const auto found = m_groups.constFind(id); found != m_groups.cend()) {
    return *found;
  }
  if (m_groups.size() >= maximumCachedNames) {
    m_groups.clear();
  }
  return *m_groups.insert(id, groupNameFor(id));
}

QString EntryFacts::itemCount(const QString &path, const QString &stamp) {
  return lookup(Fact::Items, path, stamp);
}

QString EntryFacts::dimensions(const QString &path, const QString &stamp) {
  return lookup(Fact::Dimensions, path, stamp);
}

QString EntryFacts::lookup(Fact fact, const QString &path, const QString &stamp) {
  if (!path.startsWith(QLatin1Char('/'))) {
    return {};
  }
  const QString key = (fact == Fact::Items ? QStringLiteral("items\n") : QStringLiteral("size\n")) +
                      stamp + QLatin1Char('\n') + path;
  if (const QString *answer = m_answers.object(key)) {
    return *answer;
  }
  // A full queue asks again on the next revision, when a read has finished.
  if (m_pending.contains(key) || m_pending.size() >= maximumPendingReads) {
    return {};
  }
  m_pending.insert(key);
  m_pool.start([this, fact, path, key, closing = m_closing] {
    if (*closing) {
      return;
    }
    const QString value = fact == Fact::Items ? countItems(path, *closing) : readDimensions(path);
    QMetaObject::invokeMethod(this, [this, key, value] { deliver(key, value); },
                              Qt::QueuedConnection);
  });
  return {};
}

void EntryFacts::deliver(const QString &key, const QString &value) {
  m_pending.remove(key);
  m_answers.insert(key, new QString(value));
  if (!m_publish.isActive()) {
    m_publish.start();
  }
}

} // namespace QindaQt::Apps::FileManager
