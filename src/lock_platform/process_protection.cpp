// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_protection.h"
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
namespace QindaQt::LockPlatform {
namespace {
bool managedParents(QString path) {
  for (;;) {
    struct stat st{};
    if (stat(path.toLocal8Bit().constData(), &st) || !S_ISDIR(st.st_mode) ||
        st.st_uid || (st.st_mode & (S_IWGRP | S_IWOTH))) return false;
    if (path == QStringLiteral("/")) return true;
    path = QFileInfo(path).absolutePath();
  }
}
}
bool protectAuthority() {
  const rlimit noCore{0, 0};
  return !setrlimit(RLIMIT_CORE, &noCore) && !prctl(PR_SET_DUMPABLE, 0) && !prctl(PR_GET_DUMPABLE);
}
bool restrictedPtracePolicy() {
  QFile file(QStringLiteral("/proc/sys/kernel/yama/ptrace_scope"));
  if (!file.open(QIODevice::ReadOnly)) return false;
  bool valid = false; const int mode = file.readAll().trimmed().toInt(&valid);
  return valid && mode >= 1 && mode <= 3;
}
bool rootManagedPath(const QString &path, bool directory) {
  const QFileInfo file(path); const auto canonical = file.canonicalFilePath();
  struct stat st{};
  return !canonical.isEmpty() && !stat(canonical.toLocal8Bit().constData(), &st) &&
         (directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode)) && !st.st_uid &&
         !(st.st_mode & (S_IWGRP | S_IWOTH)) && managedParents(file.absolutePath()) &&
         managedParents(QFileInfo(canonical).absolutePath());
}
bool trustedQtPaths() {
  return rootManagedPath(QLibraryInfo::path(QLibraryInfo::PluginsPath), true) &&
         rootManagedPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath), true);
}
}
