// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

namespace QindaQt::Controllers::ProcFiles {
// Bounded, non-following readers for /proc/<pid>/* files (ADR-0428).
//
// AGENT-GUARD (ADR-0397): these run only on the observation worker. A process
// map or environment can be megabytes, so every read has a byte limit, and each
// loop polls QThread::isInterruptionRequested() so teardown joins promptly.
// Files are opened O_NOFOLLOW and symlinks are read with readlink(), never
// followed: a /proc link can name a network file or a deleted target.
// Read granularity. Shared with the tests that place a needle across the seam.
inline constexpr qsizetype ChunkBytes = 64 * 1024;
inline constexpr qint64 SmallLimit = 4096;
inline constexpr qint64 EnvironLimit = 1 << 20;
// A 4 MiB maps file is roughly 40,000 mappings; a larger one is not a program
// that can be classified cheaply and is treated as unclassified.
inline constexpr qint64 MapsLimit = 4 << 20;

// Contents of `path`, cut at `limit` bytes. Empty when the file cannot be
// opened or read (vanished process, no ptrace-read access, hidepid).
QByteArray read(const QString &path, qint64 limit);
// Streams `path` and returns a bitmask of `needles` found (bit i is needles[i];
// at most 32 needles) within the first `limit` bytes. Stops at the first chunk
// after any bit of `stopMask` is set. Needles that straddle a chunk are found.
quint32 findAny(const QString &path, const QList<QByteArray> &needles, quint32 stopMask, qint64 limit);
// Text of the symlink at `path`, not followed or canonicalized; empty when it
// cannot be read.
QString linkTarget(const QString &path);
// The cgroup v2 path (the "0::" line) of the process whose proc directory is
// `processDirectory`, for example "/proc/42" or "/proc/self". Empty when the
// file is unreadable or the host has no unified hierarchy.
QString cgroupPath(const QString &processDirectory);
} // namespace QindaQt::Controllers::ProcFiles
