// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_choices.h"

#include <QHash>
#include <QVariantMap>

namespace QindaQt::QindaLutris {

QVariantList protonChoicesFor(const QVector<ProtonBuild> &builds,
                              const std::optional<ProtonBuild> &defaultBuild) {
  QStringList labels;
  QHash<QString, int> displayCount;
  for (const ProtonBuild &build : builds) {
    ++displayCount[build.displayName];
  }
  QHash<QString, int> labelCount;
  for (const ProtonBuild &build : builds) {
    QString label = build.displayName;
    if (displayCount.value(build.displayName) > 1) {
      label = QStringLiteral("%1 (%2, %3)")
                  .arg(build.displayName, build.name, protonOriginId(build.origin));
    }
    labels.append(label);
    ++labelCount[label];
  }
  QVariantList out;
  for (qsizetype i = 0; i < builds.size(); ++i) {
    const ProtonBuild &build = builds.at(i);
    QString label = labels.at(i);
    if (labelCount.value(label) > 1) {
      label += QStringLiteral(" — ") + build.path; // same name, two libraries
    }
    const bool isDefault = defaultBuild.has_value() && *defaultBuild == build;
    QVariantMap entry;
    entry.insert(QStringLiteral("name"), label);
    entry.insert(QStringLiteral("path"), build.path);
    entry.insert(QStringLiteral("build"), build.name);
    entry.insert(QStringLiteral("version"), protonVersionLabel(build.versionText));
    entry.insert(QStringLiteral("origin"), protonOriginId(build.origin));
    entry.insert(QStringLiteral("removable"), build.removable);
    entry.insert(QStringLiteral("pinnable"), build.pinnable);
    entry.insert(QStringLiteral("status"), protonBuildStatusLabel(build));
    entry.insert(QStringLiteral("isDefault"), isDefault);
    out.insert(isDefault ? 0 : out.size(), entry); // default first
  }
  return out;
}

} // namespace QindaQt::QindaLutris
