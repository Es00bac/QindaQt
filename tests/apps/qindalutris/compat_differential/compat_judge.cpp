// SPDX-License-Identifier: GPL-3.0-or-later
// The C++ half of the compat-db-v1 differential test: prints
// "<ok|refused|absent> <path>" for each file, judged by loadCompatDatabase
// against the clock given with --now (run_differential.py compares this with
// the Python validator and with expected.txt).
#include "compat_db.h"

#include <QCoreApplication>
#include <QTimeZone>

#include <cstdio>

using namespace QindaQt::QindaLutris;

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  if (argc < 3 || QByteArray(argv[1]) != "--now") {
    std::fprintf(stderr, "usage: compat_judge --now YYYY-MM-DDTHH:MM:SSZ FILE...\n");
    return 2;
  }
  QDateTime now = QDateTime::fromString(QString::fromLatin1(argv[2]), Qt::ISODate);
  now.setTimeZone(QTimeZone::UTC);
  if (!now.isValid()) return 2;
  for (int i = 3; i < argc; ++i) {
    CompatLoadError error = CompatLoadError::None;
    const CompatDatabase db = loadCompatDatabase(QString::fromLocal8Bit(argv[i]), &error, now);
    const char *verdict = db.isLoaded() ? "ok"
                        : error == CompatLoadError::Absent ? "absent" : "refused";
    std::printf("%s %s\n", verdict, argv[i]);
  }
  return 0;
}
