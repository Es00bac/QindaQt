// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sys/stat.h>
namespace QindaQt::Services::AndroidRuntime::ProcessLifetime {
// Borrowed descriptors: owners serialize access and keep each held until retirement.
// These check the supplied lifetime/object, not installed-code or renderer-role admission.
bool pidfdMatches(int fd,int pid);
bool pinExecutable(int fd,struct stat &pin);
bool live(int lifetimeFd,int pid,const struct stat &pin);
} // namespace QindaQt::Services::AndroidRuntime::ProcessLifetime
