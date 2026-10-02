// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "print_conversion.h"
#include <QIODevice>
#include <functional>
namespace QindaQt::Services::Portal {
// Copies the sandbox descriptor without changing its shared offset. Print data
// are bounded at 512 MiB and no paths are derived from application options.
// Process runner is injected only by native test/composition code.
using PrintRunner = std::function<bool(const QString &, const QStringList &, QIODevice &)>;
bool submitPrint(QPrinter &, int fd, const PrintRunner &runner);
bool runPrintCommand(const QString &, const QStringList &, QIODevice &);
}
