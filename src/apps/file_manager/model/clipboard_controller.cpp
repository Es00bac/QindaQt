// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboard_controller.h"

#include "../mutation/mutation_controller.h"

#include <QClipboard>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace QindaQt::Apps::FileManager {
namespace {

// GNOME/KDE cut-vs-copy interop markers. Payload is "copy" or "cut" on the
// first line followed by one file URL per line; KDE additionally carries a
// boolean flag under its own type.
constexpr auto kGnomeCopiedFilesType = "x-special/gnome-copied-files";
constexpr auto kKdeCutSelectionType = "application/x-kde-cutselection";

[[nodiscard]] QStringList pathsOf(const QVariantList &items) {
  QStringList paths;
  paths.reserve(items.size());
  for (const QVariant &item : items) {
    paths.append(item.toMap().value(QStringLiteral("path")).toString());
  }
  return paths;
}

} // namespace

ClipboardController::ClipboardController(MutationController &mutation,
                                         QClipboard &clipboard, QObject *parent)
    : QObject(parent), m_mutation(mutation), m_clipboard(clipboard) {
  QObject::connect(&clipboard, &QClipboard::changed, this,
                   [this](QClipboard::Mode mode) {
                     if (mode != QClipboard::Clipboard) {
                       return;
                     }
                     if (m_clipboard.ownsClipboard()) {
                       return;
                     }
                     // AGENT-NOTE: some platform clipboards (the offscreen
                     // test QPA among them) never report ownership back for
                     // data this process just published, so ownsClipboard()
                     // alone cannot distinguish a foreign takeover from our
                     // own publish. A payload identical to the owned snapshot
                     // is treated as still ours; only genuinely different
                     // content clears the snapshot and is adopted as foreign.
                     const QMimeData *mime = m_clipboard.mimeData();
                     QVariantList incomingUrls;
                     if (mime != nullptr) {
                       for (const QUrl &url : mime->urls()) {
                         incomingUrls.append(url);
                       }
                     }
                     if (!m_snapshot.isEmpty() &&
                         localPathsFromUrls(incomingUrls) == pathsOf(m_snapshot)) {
                       return;
                     }
                     // Another source now owns the clipboard: the owned
                     // snapshot is stale, and only the new payload decides
                     // whether paste is offered.
                     m_snapshot.clear();
                     m_cut = false;
                     m_pendingCutPaste = false;
                     adoptForeignClipboard();
                     emit stateChanged();
                   });
  QObject::connect(&m_mutation, &MutationController::stateChanged, this, [this] {
    // A committed cut+paste move leaves dangling sources on the clipboard;
    // clear once the operation finishes successfully. A failed or cancelled
    // paste keeps the cut snapshot so the user can retry elsewhere.
    if (m_pendingCutPaste && !m_mutation.busy()) {
      m_pendingCutPaste = false;
      if (m_mutation.failureCode() == QLatin1String("none")) {
        clear();
        return;
      }
    }
    emit stateChanged();
  });
  adoptForeignClipboard();
}

bool ClipboardController::canPaste() const {
  return !m_snapshot.isEmpty() || !m_foreignPaths.isEmpty();
}

QString ClipboardController::mode() const {
  if (!m_snapshot.isEmpty()) {
    return m_cut ? QStringLiteral("cut") : QStringLiteral("copy");
  }
  if (!m_foreignPaths.isEmpty()) {
    return QStringLiteral("copy");
  }
  return QStringLiteral("none");
}

int ClipboardController::count() const {
  return m_snapshot.isEmpty() ? static_cast<int>(m_foreignPaths.size())
                              : static_cast<int>(m_snapshot.size());
}

bool ClipboardController::copySelection(const QVariantList &items) {
  if (items.isEmpty()) {
    return reject(QStringLiteral("No items are selected"));
  }
  m_snapshot = items;
  m_cut = false;
  m_foreignPaths.clear();
  publishSnapshot(false);
  emit stateChanged();
  return true;
}

bool ClipboardController::cutSelection(const QVariantList &items) {
  if (items.isEmpty()) {
    return reject(QStringLiteral("No items are selected"));
  }
  m_snapshot = items;
  m_cut = true;
  m_foreignPaths.clear();
  publishSnapshot(true);
  emit stateChanged();
  return true;
}

bool ClipboardController::pasteInto(const QString &destinationDirectory) {
  const QFileInfo destinationInfo(destinationDirectory);
  if (!destinationInfo.isAbsolute() || !destinationInfo.isDir()) {
    return reject(QStringLiteral("Choose an existing folder as the paste destination"));
  }
  const QString destination = destinationInfo.absoluteFilePath();
  if (!m_snapshot.isEmpty()) {
    return dispatchOwn(destination);
  }
  if (m_foreignPaths.isEmpty()) {
    return reject(QStringLiteral("The clipboard holds no files to paste"));
  }
  m_lastRejection.clear();
  return m_mutation.copyForeignPathsTo(m_foreignPaths, destination);
}

bool ClipboardController::dropUrlsInto(const QVariantList &urls,
                                       const QString &destinationDirectory) {
  const QStringList paths = localPathsFromUrls(urls);
  if (paths.isEmpty()) {
    return reject(QStringLiteral("Only local files can be dropped here"));
  }
  const QFileInfo destinationInfo(destinationDirectory);
  if (!destinationInfo.isAbsolute() || !destinationInfo.isDir()) {
    return reject(QStringLiteral("Choose an existing folder as the drop destination"));
  }
  m_lastRejection.clear();
  return m_mutation.copyForeignPathsTo(paths, destinationInfo.absoluteFilePath());
}

void ClipboardController::clear() {
  const bool hadContent = canPaste();
  m_snapshot.clear();
  m_cut = false;
  m_pendingCutPaste = false;
  m_foreignPaths.clear();
  if (m_clipboard.ownsClipboard()) {
    m_clipboard.clear();
  }
  if (hadContent) {
    emit stateChanged();
  }
}

void ClipboardController::setSelectionCount(int count) {
  if (m_selectionCount == count) {
    return;
  }
  m_selectionCount = count;
  emit stateChanged();
}

QStringList ClipboardController::localPathsFromUrls(const QVariantList &urls) {
  QStringList paths;
  paths.reserve(qMin(urls.size(), MutationController::maximumForeignPaths));
  for (const QVariant &entry : urls) {
    if (paths.size() >= MutationController::maximumForeignPaths) {
      break;
    }
    const QUrl url = entry.toUrl();
    if (!url.isValid() || !url.isLocalFile()) {
      continue;
    }
    const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    if (!paths.contains(path)) {
      paths.append(path);
    }
  }
  return paths;
}

void ClipboardController::adoptForeignClipboard() {
  m_foreignPaths.clear();
  const QMimeData *mime = m_clipboard.mimeData();
  if (mime == nullptr || !mime->hasUrls()) {
    return;
  }
  QVariantList urls;
  for (const QUrl &url : mime->urls()) {
    urls.append(url);
  }
  m_foreignPaths = localPathsFromUrls(urls);
}

void ClipboardController::publishSnapshot(bool cut) {
  const QStringList paths = pathsOf(m_snapshot);
  QList<QUrl> urls;
  urls.reserve(paths.size());
  QByteArray gnomePayload = cut ? QByteArrayLiteral("cut\n") : QByteArrayLiteral("copy\n");
  for (const QString &path : paths) {
    const QUrl url = QUrl::fromLocalFile(path);
    urls.append(url);
    gnomePayload += url.toEncoded() + '\n';
  }
  auto *mime = new QMimeData;
  mime->setUrls(urls);
  mime->setData(QLatin1String(kGnomeCopiedFilesType), gnomePayload);
  if (cut) {
    mime->setData(QLatin1String(kKdeCutSelectionType), QByteArrayLiteral("1"));
  }
  m_clipboard.setMimeData(mime);
}

bool ClipboardController::dispatchOwn(const QString &destinationDirectory) {
  // Paste-into-self guards: an entry dropped on its own parent is an
  // already-exists no-op; a folder pasted into itself or its own descendant
  // would copy recursively into the tree being walked. Both are refused
  // before dispatch rather than reported as a mid-batch failure.
  QVariantList accepted;
  for (const QVariant &item : m_snapshot) {
    const QString source = item.toMap().value(QStringLiteral("path")).toString();
    if (source == destinationDirectory ||
        destinationDirectory.startsWith(source + QLatin1Char('/')) ||
        QFileInfo(source).absolutePath() == destinationDirectory) {
      continue;
    }
    accepted.append(item);
  }
  if (accepted.isEmpty()) {
    return reject(QStringLiteral("Nothing to paste into this folder"));
  }
  m_lastRejection.clear();
  m_pendingCutPaste = m_cut;
  const bool dispatched = m_cut
      ? m_mutation.moveItemsTo(accepted, destinationDirectory)
      : m_mutation.copyItemsTo(accepted, destinationDirectory);
  if (!dispatched) {
    m_pendingCutPaste = false;
  }
  emit stateChanged();
  return dispatched;
}

bool ClipboardController::reject(const QString &reason) {
  m_lastRejection = reason;
  emit stateChanged();
  return false;
}

} // namespace QindaQt::Apps::FileManager
