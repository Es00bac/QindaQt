// SPDX-License-Identifier: GPL-3.0-or-later
// KArchiveCodec::extract, split from karchive_codec.cpp (Compress) to keep
// both under the source-shape budget; the shared helpers are karchive_io.h.
#include "karchive_codec.h"

#include "karchive_io.h"

#include <KCompressionDevice>
#include <KTar>
#include <KZip>

#include <algorithm>
#include <array>
#include <memory>

namespace QindaQt::Apps::FileManager {

using namespace KArchiveIo;

namespace {

struct PlannedEntry {
  QStringList path;
  const KArchiveEntry *entry = nullptr;
};

struct ExtractPlan {
  QVector<PlannedEntry> folders;
  QVector<PlannedEntry> files;
  QVector<PlannedEntry> links;

  [[nodiscard]] qsizetype size() const { return folders.size() + files.size() + links.size(); }
};

// AGENT-GUARD: an archive names its entries; only a single, ordinary path
// component may become a name on disk. "..", "." and separators are how an
// archive escapes its destination, so they fail the whole Extract.
[[nodiscard]] bool safeComponent(const QString &name) {
  return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") &&
         !name.contains(QLatin1Char('/')) && !name.contains(QChar::Null);
}

[[nodiscard]] MutationResult planEntries(const KArchiveDirectory &directory,
                                         const QStringList &prefix, int depth, ExtractPlan *plan) {
  if (depth > KArchiveCodec::maximumDepth) {
    return failure(MutationError::Unsupported, QStringLiteral("The archive is nested too deeply"));
  }
  QStringList names = directory.entries();
  names.sort();
  for (const QString &name : names) {
    const KArchiveEntry *entry = directory.entry(name);
    if (entry == nullptr) {
      continue;
    }
    if (!safeComponent(name)) {
      return failure(MutationError::SymlinkEscape,
                     QStringLiteral("The archive contains an unsafe name: %1").arg(name));
    }
    if (plan->size() >= KArchiveCodec::maximumEntries) {
      return failure(MutationError::Unsupported,
                     QStringLiteral("The archive exceeds the item safety bound"));
    }
    const PlannedEntry planned{prefix + QStringList{name}, entry};
    if (!entry->symLinkTarget().isEmpty()) {
      plan->links.append(planned);
    } else if (entry->isDirectory()) {
      plan->folders.append(planned);
      const auto nested = planEntries(*static_cast<const KArchiveDirectory *>(entry),
                                      planned.path, depth + 1, plan);
      if (!nested.ok()) {
        return nested;
      }
    } else if (entry->isFile()) {
      plan->files.append(planned);
    }
  }
  return {};
}

// Opens the folder that will hold `path`'s last component, walking down from
// the destination without following links.
[[nodiscard]] UniqueFd openParentOf(int root, const QStringList &path) {
  UniqueFd current(::fcntl(root, F_DUPFD_CLOEXEC, 0));
  for (qsizetype index = 0; current.valid() && index + 1 < path.size(); ++index) {
    current = UniqueFd(
        ::openat(current.get(), QFile::encodeName(path.at(index)).constData(), directoryFlags));
  }
  return current;
}

[[nodiscard]] MutationResult unreachable(const PlannedEntry &planned) {
  return failure(mutationErrorForErrno(errno),
                 QStringLiteral("“%1” could not be extracted").arg(planned.path.join(QLatin1Char('/'))));
}

[[nodiscard]] MutationResult extractFile(int root, const PlannedEntry &planned,
                                         const MutationCancellation &cancellation) {
  const auto *file = static_cast<const KArchiveFile *>(planned.entry);
  UniqueFd parent = openParentOf(root, planned.path);
  const mode_t mode = (file->permissions() & 0777) != 0 ? (file->permissions() & 0777) : 0644;
  UniqueFd output(parent.valid()
                      ? ::openat(parent.get(), QFile::encodeName(planned.path.constLast()).constData(),
                                 O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode)
                      : -1);
  if (!output.valid()) {
    return unreachable(planned);
  }
  const std::unique_ptr<QIODevice> data(file->createDevice());
  if (!data || (!data->isOpen() && !data->open(QIODevice::ReadOnly))) {
    return failure(MutationError::IoError, QStringLiteral("The archive could not be read"));
  }
  std::array<char, 64 * 1024> buffer{};
  qint64 total = 0;
  while (true) {
    if (cancelled(cancellation)) {
      return failure(MutationError::Cancelled, QStringLiteral("Extract cancelled"));
    }
    const qint64 count = data->read(buffer.data(), buffer.size());
    if (count < 0) {
      return failure(MutationError::IoError, QStringLiteral("The archive is damaged"));
    }
    if (count == 0) {
      break;
    }
    if (!writeAll(output.get(), buffer.data(), count)) {
      return unreachable(planned);
    }
    total += count;
  }
  if (total != file->size()) {
    return failure(MutationError::IoError, QStringLiteral("The archive is damaged"));
  }
  if (const QDateTime date = file->date(); date.isValid()) {
    const struct timespec times[2] = {{0, UTIME_OMIT},
                                      {static_cast<time_t>(date.toSecsSinceEpoch()), 0}};
    const int ignoredTimes = ::futimens(output.get(), times);
    Q_UNUSED(ignoredTimes);
  }
  return {};
}

[[nodiscard]] MutationResult extractPlanned(int root, const ExtractPlan &plan,
                                            const MutationCancellation &cancellation,
                                            const MutationProgressCallback &progress) {
  const int total = static_cast<int>(plan.size());
  int done = 0;
  // Reports one finished entry; false once the user has cancelled.
  const auto step = [&]() {
    ++done;
    if (progress) {
      progress({done, total, QStringLiteral("Extracted %1 of %2 items").arg(done).arg(total)});
    }
    return !cancelled(cancellation);
  };
  const MutationResult stopped =
      failure(MutationError::Cancelled, QStringLiteral("Extract cancelled"));
  // Folders first, in depth-first order, so every parent exists before its
  // children; owner access is kept so the extraction itself can fill them.
  for (const PlannedEntry &folder : plan.folders) {
    UniqueFd parent = openParentOf(root, folder.path);
    const mode_t mode = (folder.entry->permissions() & 0777) | 0700;
    if (!parent.valid() || ::mkdirat(parent.get(), QFile::encodeName(folder.path.constLast()).constData(),
                                     mode) != 0) {
      return unreachable(folder);
    }
    if (!step()) {
      return stopped;
    }
  }
  // Files in archive order: a compressed tar is then read front to back once.
  QVector<PlannedEntry> files = plan.files;
  std::stable_sort(files.begin(), files.end(), [](const PlannedEntry &left, const PlannedEntry &right) {
    return static_cast<const KArchiveFile *>(left.entry)->position() <
           static_cast<const KArchiveFile *>(right.entry)->position();
  });
  for (const PlannedEntry &file : std::as_const(files)) {
    if (const auto extracted = extractFile(root, file, cancellation); !extracted.ok()) {
      return extracted;
    }
    if (!step()) {
      return stopped;
    }
  }
  // Links last, so no later entry is ever written through one.
  for (const PlannedEntry &link : plan.links) {
    const QByteArray target = QFile::encodeName(link.entry->symLinkTarget());
    UniqueFd parent = openParentOf(root, link.path);
    if (target.contains('\0') || !parent.valid() ||
        ::symlinkat(target.constData(), parent.get(),
                    QFile::encodeName(link.path.constLast()).constData()) != 0) {
      return unreachable(link);
    }
    if (!step()) {
      return stopped;
    }
  }
  return {};
}

[[nodiscard]] KCompressionDevice::CompressionType compressionFor(ArchiveFormat format) {
  switch (format) {
  case ArchiveFormat::TarGzip:
    return KCompressionDevice::GZip;
  case ArchiveFormat::TarBzip2:
    return KCompressionDevice::BZip2;
  case ArchiveFormat::TarXz:
    return KCompressionDevice::Xz;
  case ArchiveFormat::TarZstd:
    return KCompressionDevice::Zstd;
  case ArchiveFormat::None:
  case ArchiveFormat::Zip:
  case ArchiveFormat::Tar:
    break;
  }
  return KCompressionDevice::None;
}

} // namespace

MutationResult KArchiveCodec::extract(const QString &archivePath, const FileIdentity &expectedArchive,
                                      const QString &destination,
                                      const MutationCancellation &cancellation,
                                      const MutationProgressCallback &progress) {
  const ArchiveFormat format = archiveFormatForName(QFileInfo(archivePath).fileName());
  if (format == ArchiveFormat::None) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("This is not an archive File Manager can extract"));
  }
  UniqueFd folder = openAbsoluteDirectory(QFileInfo(archivePath).absolutePath());
  UniqueFd input(folder.valid() ? ::openat(folder.get(),
                                           QFile::encodeName(QFileInfo(archivePath).fileName()).constData(),
                                           O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
                                : -1);
  struct stat opened {};
  if (!input.valid() || ::fstat(input.get(), &opened) != 0) {
    return failure(mutationErrorForErrno(errno), QStringLiteral("The archive could not be opened safely"));
  }
  if (!S_ISREG(opened.st_mode) || identityOf(opened) != expectedArchive) {
    return failure(MutationError::Changed,
                   QStringLiteral("The archive changed before it could be extracted"));
  }
  UniqueFd root = openAbsoluteDirectory(destination);
  if (!root.valid()) {
    return failure(mutationErrorForErrno(errno),
                   QStringLiteral("The destination folder could not be opened safely"));
  }
  // Declared in this order so the archive is destroyed before the
  // decompressor, and both before the file they read.
  QFile file;
  std::unique_ptr<KCompressionDevice> decompressor;
  std::unique_ptr<KArchive> archive;
  if (!file.open(input.get(), QIODevice::ReadOnly, QFileDevice::DontCloseHandle)) {
    return failure(MutationError::IoError, QStringLiteral("The archive could not be read"));
  }
  if (format == ArchiveFormat::Zip) {
    archive = std::make_unique<KZip>(&file);
  } else if (format == ArchiveFormat::Tar) {
    archive = std::make_unique<KTar>(&file);
  } else {
    decompressor = std::make_unique<KCompressionDevice>(&file, false, compressionFor(format));
    archive = std::make_unique<KTar>(decompressor.get());
  }
  if (!archive->open(QIODevice::ReadOnly) || archive->directory() == nullptr) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("The archive could not be read: %1").arg(archive->errorString()));
  }
  // An archive whose root holds just one folder ("photos.zip" holding
  // "photos/") unpacks that folder's contents, so the new folder named after
  // the archive does not hold a second folder of the same thing.
  const KArchiveDirectory *top = archive->directory();
  if (const QStringList rootNames = top->entries(); rootNames.size() == 1) {
    const KArchiveEntry *only = top->entry(rootNames.constFirst());
    if (only != nullptr && only->isDirectory() && only->symLinkTarget().isEmpty() &&
        safeComponent(rootNames.constFirst())) {
      top = static_cast<const KArchiveDirectory *>(only);
    }
  }
  ExtractPlan plan;
  if (const auto planned = planEntries(*top, {}, 0, &plan); !planned.ok()) {
    return planned;
  }
  MutationResult result = extractPlanned(root.get(), plan, cancellation, progress);
  if (result.ok()) {
    result.outputPath = destination;
    result.diagnostic = QStringLiteral("Extracted %1 items into “%2”")
                            .arg(plan.size())
                            .arg(QFileInfo(destination).fileName());
  }
  return result;
}

} // namespace QindaQt::Apps::FileManager
