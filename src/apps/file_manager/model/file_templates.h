// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::FileManager {

// ADR-0269: New File's templates -- the regular files directly inside the
// user's templates folder (XDG_TEMPLATES_DIR, usually ~/Templates), A to Z.
// Hidden files, folders and links are skipped. refresh() is one bounded
// synchronous listing through LocalDirectoryLister; nothing is created here
// (MutationController::createFile copies the chosen template). GUI-thread only.
class FileTemplates final : public QObject {
  Q_OBJECT
  // {name (without extension), fileName, path} maps.
  Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged FINAL)

public:
  static constexpr int maximumTemplates = 64;

  // An empty directory (no templates folder) lists nothing.
  explicit FileTemplates(QString directory, QObject *parent = nullptr);

  Q_INVOKABLE void refresh();
  [[nodiscard]] QVariantList templates() const { return m_templates; }

  // The XDG templates folder, or empty when it is unset or is the home
  // folder itself (the xdg-user-dirs way of switching it off).
  [[nodiscard]] static QString userTemplatesDirectory();

signals:
  void templatesChanged();

private:
  QString m_directory;
  QVariantList m_templates;
};

} // namespace QindaQt::Apps::FileManager
