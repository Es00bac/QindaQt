// SPDX-License-Identifier: LGPL-3.0-or-later

#include "procfs_reader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>

#include <chrono>
#include <pwd.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace QindaQt::SystemMonitor {
namespace {

std::optional<QByteArray> readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  return file.readAll();
}

QVector<QByteArray> fields(const QByteArray &line) {
  return line.simplified().split(' ');
}

quint64 unsignedValue(const QByteArray &text, bool *ok = nullptr) {
  bool converted = false;
  const quint64 value = text.toULongLong(&converted);
  if (ok) {
    *ok = converted;
  }
  return value;
}

CpuCounters parseCpu(const QByteArray &line) {
  const auto values = fields(line);
  CpuCounters cpu;
  if (values.size() < 5) {
    return cpu;
  }
  cpu.id = QString::fromLatin1(values[0]);
  // guest and guest_nice are already included in user/nice by Linux.
  for (int i = 1; i < values.size() && i <= 8; ++i) {
    cpu.total += unsignedValue(values[i]);
  }
  cpu.idle = unsignedValue(values[4]);
  if (values.size() > 5) {
    cpu.idle += unsignedValue(values[5]);
  }
  return cpu;
}

QString userName(uid_t uid) {
  static QMutex cacheMutex;
  static QHash<uid_t, QString> cache;
  const QMutexLocker locker(&cacheMutex);
  if (const auto found = cache.constFind(uid); found != cache.cend()) {
    return found.value();
  }
  long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (suggested < 1024) {
    suggested = 16384;
  }
  QByteArray storage(suggested, Qt::Uninitialized);
  passwd entry{};
  passwd *result = nullptr;
  if (getpwuid_r(uid, &entry, storage.data(), size_t(storage.size()),
                 &result) == 0 &&
      result) {
    const QString name = QString::fromLocal8Bit(result->pw_name);
    cache.insert(uid, name);
    return name;
  }
  const QString numeric = QString::number(uid);
  cache.insert(uid, numeric);
  return numeric;
}

std::optional<ProcessCounters> parseProcess(const QString &procRoot, qint64 pid,
                                            QString *error) {
  const QString directory = procRoot + QLatin1Char('/') + QString::number(pid);
  const auto statBytes = readFile(directory + QStringLiteral("/stat"));
  if (!statBytes) {
    if (error) {
      *error = QStringLiteral("Cannot read process %1 stat data").arg(pid);
    }
    return std::nullopt;
  }

  const qsizetype opening = statBytes->indexOf('(');
  const qsizetype closing = statBytes->lastIndexOf(')');
  if (opening < 0 || closing <= opening || closing + 2 >= statBytes->size()) {
    if (error) {
      *error = QStringLiteral("Malformed process %1 stat data").arg(pid);
    }
    return std::nullopt;
  }
  const auto tail = fields(statBytes->mid(closing + 2));
  if (tail.size() < 22) {
    if (error) {
      *error = QStringLiteral("Incomplete process %1 stat data").arg(pid);
    }
    return std::nullopt;
  }

  bool startOk = false;
  ProcessCounters process;
  process.pid = pid;
  process.name = QString::fromLocal8Bit(
      statBytes->mid(opening + 1, closing - opening - 1));
  process.state = QString::fromLatin1(tail[0]);
  quint64 userTicks = unsignedValue(tail[11]);
  if (tail.size() > 39) {
    const quint64 guestTicks = unsignedValue(tail[39]);
    userTicks = userTicks >= guestTicks ? userTicks - guestTicks : 0;
  }
  process.cpuTicks = userTicks + unsignedValue(tail[12]);
  process.nice = tail[16].toInt();
  process.threads = tail[17].toInt();
  process.startTicks = unsignedValue(tail[19], &startOk);
  if (!startOk) {
    if (error) {
      *error = QStringLiteral("Invalid process %1 start time").arg(pid);
    }
    return std::nullopt;
  }

  if (const auto status = readFile(directory + QStringLiteral("/status"))) {
    const auto lines = status->split('\n');
    for (const auto &line : lines) {
      if (line.startsWith("Uid:")) {
        const auto values = fields(line);
        if (values.size() >= 2) {
          process.user = userName(uid_t(unsignedValue(values[1])));
        }
      } else if (line.startsWith("VmRSS:")) {
        const auto values = fields(line);
        if (values.size() >= 2) {
          process.memoryBytes = unsignedValue(values[1]) * 1024;
          process.memoryAvailable = true;
        }
      }
    }
  }
  if (const auto io = readFile(directory + QStringLiteral("/io"))) {
    for (const auto &line : io->split('\n')) {
      if (line.startsWith("read_bytes:")) {
        bool converted = false;
        const quint64 value = unsignedValue(
            line.mid(sizeof("read_bytes:") - 1).trimmed(), &converted);
        if (converted) {
          process.readBytes = value;
          process.readBytesAvailable = true;
        }
      } else if (line.startsWith("write_bytes:")) {
        bool converted = false;
        const quint64 value = unsignedValue(
            line.mid(sizeof("write_bytes:") - 1).trimmed(), &converted);
        if (converted) {
          process.writeBytes = value;
          process.writeBytesAvailable = true;
        }
      }
    }
  }
  if (const auto command = readFile(directory + QStringLiteral("/cmdline"))) {
    QByteArray normalized = *command;
    normalized.replace('\0', ' ');
    process.command = QString::fromLocal8Bit(normalized.trimmed());
  }
  if (process.command.isEmpty()) {
    process.command = process.name;
  }
  return process;
}

bool isRemoteFilesystem(const QByteArray &type, const QByteArray &device) {
  static const QList<QByteArray> remoteTypes = {
      "9p",        "afs",   "ceph", "cifs", "fuse", "fuse.sshfs",
      "glusterfs", "ncpfs", "nfs",  "nfs4", "smb3", "sshfs",
  };
  return remoteTypes.contains(type) || type.startsWith("fuse.") ||
         device.startsWith("//");
}

QString decodeMountField(QByteArray field) {
  field.replace("\\040", " ");
  field.replace("\\011", "\t");
  field.replace("\\012", "\n");
  field.replace("\\134", "\\");
  return QString::fromLocal8Bit(field);
}

QVector<FilesystemSample> readFilesystems(const QString &procRoot) {
  QVector<FilesystemSample> result;
  const auto mounts = readFile(procRoot + QStringLiteral("/mounts"));
  if (!mounts) {
    return result;
  }
  QSet<QString> seen;
  for (const auto &line : mounts->split('\n')) {
    const auto values = fields(line);
    if (values.size() < 3 || isRemoteFilesystem(values[2], values[0])) {
      continue;
    }
    const QString path = decodeMountField(values[1]);
    if (seen.contains(path)) {
      continue;
    }
    struct statvfs stats{};
    const QByteArray encodedPath = QFile::encodeName(path);
    if (::statvfs(encodedPath.constData(), &stats) != 0) {
      continue;
    }
    const quint64 blockSize = stats.f_frsize ? stats.f_frsize : stats.f_bsize;
    FilesystemSample sample;
    sample.path = path;
    sample.device = decodeMountField(values[0]);
    sample.total = quint64(stats.f_blocks) * blockSize;
    sample.available = quint64(stats.f_bavail) * blockSize;
    const quint64 free = quint64(stats.f_bfree) * blockSize;
    sample.used = sample.total >= free ? sample.total - free : 0;
    result.push_back(std::move(sample));
    seen.insert(path);
  }
  return result;
}

bool isDiskPartition(const QString &procRoot, const QByteArray &major,
                     const QByteArray &minor, const QString &name) {
  if (procRoot == QStringLiteral("/proc")) {
    const QString devicePath =
        QStringLiteral("/sys/dev/block/%1:%2")
            .arg(QString::fromLatin1(major), QString::fromLatin1(minor));
    if (QFileInfo::exists(devicePath)) {
      return QFileInfo::exists(devicePath + QStringLiteral("/partition"));
    }
  }
  static const QRegularExpression partitionPattern(
      QStringLiteral("^(?:sd[a-z]+|vd[a-z]+|xvd[a-z]+)\\d+$|^nvme\\d+n\\d+p\\d+"
                     "$|^mmcblk\\d+p\\d+$"));
  return partitionPattern.match(name).hasMatch();
}

} // namespace

ProcfsReader::ProcfsReader(QString procRoot)
    : m_procRoot(std::move(procRoot)) {}

RawSample ProcfsReader::read(QString *error) const {
  if (error) {
    error->clear();
  }
  RawSample sample;
  sample.monotonicNanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  sample.timestampMilliseconds = QDateTime::currentMSecsSinceEpoch();

  const auto stat = readFile(m_procRoot + QStringLiteral("/stat"));
  const auto memory = readFile(m_procRoot + QStringLiteral("/meminfo"));
  if (!stat || !memory) {
    if (error) {
      *error = QStringLiteral(
                   "Cannot read required procfs CPU or memory data from %1")
                   .arg(m_procRoot);
    }
    return sample;
  }

  for (const auto &line : stat->split('\n')) {
    if (line.startsWith("cpu ")) {
      sample.cpu = parseCpu(line);
    } else if (line.startsWith("cpu") && line.size() > 3 && line[3] >= '0' &&
               line[3] <= '9') {
      auto core = parseCpu(line);
      core.id.remove(0, 3);
      sample.cores.push_back(std::move(core));
    }
  }

  QHash<QByteArray, quint64> memoryValues;
  for (const auto &line : memory->split('\n')) {
    const qsizetype colon = line.indexOf(':');
    if (colon > 0) {
      memoryValues.insert(
          line.left(colon),
          unsignedValue(line.mid(colon + 1).simplified().split(' ').value(0)) *
              1024);
    }
  }
  sample.memoryTotal = memoryValues.value("MemTotal");
  sample.memoryAvailable = memoryValues.value("MemAvailable");
  sample.memoryCached =
      memoryValues.value("Cached") + memoryValues.value("SReclaimable");
  sample.swapTotal = memoryValues.value("SwapTotal");
  sample.swapFree = memoryValues.value("SwapFree");
  if (sample.cpu.id.isEmpty() || sample.memoryTotal == 0) {
    if (error) {
      *error =
          QStringLiteral("Malformed required procfs CPU or memory data in %1")
              .arg(m_procRoot);
    }
    return sample;
  }

  if (const auto uptime = readFile(m_procRoot + QStringLiteral("/uptime"))) {
    sample.uptimeSeconds = fields(*uptime).value(0).toDouble();
  }
  if (const auto load = readFile(m_procRoot + QStringLiteral("/loadavg"))) {
    const auto values = fields(*load);
    sample.load1 = values.value(0).toDouble();
    sample.load5 = values.value(1).toDouble();
    sample.load15 = values.value(2).toDouble();
  }

  if (const auto diskstats =
          readFile(m_procRoot + QStringLiteral("/diskstats"))) {
    for (const auto &line : diskstats->split('\n')) {
      const auto values = fields(line);
      if (values.size() < 14) {
        continue;
      }
      const QString name = QString::fromLatin1(values[2]);
      if (isDiskPartition(m_procRoot, values[0], values[1], name)) {
        continue;
      }
      DiskCounters disk;
      disk.id = QString::fromLatin1(values[0]) + QLatin1Char(':') +
                QString::fromLatin1(values[1]);
      disk.name = name;
      disk.readBytes = unsignedValue(values[5]) * 512;
      disk.writeBytes = unsignedValue(values[9]) * 512;
      disk.busyMilliseconds = unsignedValue(values[12]);
      sample.disks.push_back(std::move(disk));
    }
  }

  if (const auto network = readFile(m_procRoot + QStringLiteral("/net/dev"))) {
    for (const auto &line : network->split('\n')) {
      const qsizetype colon = line.indexOf(':');
      if (colon < 0) {
        continue;
      }
      const auto values = fields(line.mid(colon + 1));
      if (values.size() < 16) {
        continue;
      }
      NetworkCounters interface;
      interface.name = QString::fromLatin1(line.left(colon).trimmed());
      interface.rxBytes = unsignedValue(values[0]);
      interface.txBytes = unsignedValue(values[8]);
      sample.network.push_back(std::move(interface));
    }
  }

  sample.filesystems = readFilesystems(m_procRoot);
  const auto entries =
      QDir(m_procRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const auto &entry : entries) {
    bool numeric = false;
    const qint64 pid = entry.toLongLong(&numeric);
    if (!numeric || pid <= 0) {
      continue;
    }
    if (auto process = parseProcess(m_procRoot, pid, nullptr)) {
      sample.processes.push_back(std::move(*process));
    }
  }
  return sample;
}

std::optional<ProcessCounters> ProcfsReader::readProcess(qint64 pid,
                                                         QString *error) const {
  if (error) {
    error->clear();
  }
  return parseProcess(m_procRoot, pid, error);
}

} // namespace QindaQt::SystemMonitor
