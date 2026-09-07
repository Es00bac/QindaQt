// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces_ui/workspace_dialogs.h"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSet>
#include <QUuid>
#include <QVBoxLayout>

namespace QindaQt::WorkspacesUi {
namespace {

QString windowText(const WorkspaceWindow &entry, WorkspaceUiPort &port) {
  const auto title = entry.title.isEmpty() ? QObject::tr("Untitled window")
                                           : entry.title;
  const auto application = port.installedApplication(entry.window.desktopEntryId);
  return application ? QStringLiteral("%1 — %2").arg(title, application->displayName)
                     : title;
}

QIcon swatchIcon(const QString &hex) {
  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setPen(Qt::black);
  painter.setBrush(QColor(hex));
  painter.drawRect(1, 1, 14, 14);
  return QIcon(pixmap);
}

class SaveDialog final : public QDialog {
public:
  SaveDialog(QString initialName, QString initialColor, QWidget *parent)
      : QDialog(parent) {
    setWindowTitle(tr("Save workspace"));
    auto *form = new QFormLayout(this);
    m_name.setObjectName(QStringLiteral("workspaceName"));
    m_name.setText(std::move(initialName));
    form->addRow(tr("Name:"), &m_name);
    auto *colorRow = new QWidget(this);
    auto *colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins({});
    m_swatch.setObjectName(QStringLiteral("workspaceColorSwatch"));
    m_swatch.setFixedSize(20, 20);
    colorLayout->addWidget(&m_swatch);
    colorLayout->addWidget(&m_colorName, 1);
    auto *choose = new QPushButton(tr("Choose color"), colorRow);
    choose->setObjectName(QStringLiteral("chooseColor"));
    colorLayout->addWidget(choose);
    auto *followTheme = new QPushButton(tr("Follow theme"), colorRow);
    followTheme->setObjectName(QStringLiteral("followTheme"));
    colorLayout->addWidget(followTheme);
    form->addRow(tr("Color:"), colorRow);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                          QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setObjectName(
        QStringLiteral("confirmSave"));
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
      if (m_name.text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Enter a workspace name."));
        return;
      }
      accept();
    });
    connect(choose, &QPushButton::clicked, this, [this] {
      const auto selected = QColorDialog::getColor(
          m_color.isEmpty() ? palette().color(QPalette::Accent) : QColor(m_color),
          this, tr("Choose workspace color"));
      if (selected.isValid())
        setColor(selected);
    });
    connect(followTheme, &QPushButton::clicked, this, [this] { setColor({}); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setColor(QColor(initialColor));
  }
  [[nodiscard]] QString name() const { return m_name.text().trimmed(); }
  [[nodiscard]] QString color() const { return m_color; }

private:
  void setColor(const QColor &color) {
    m_color = color.isValid() ? color.name(QColor::HexRgb).toUpper() : QString();
    m_colorName.setText(m_color.isEmpty() ? tr("Follow theme") : tr("Custom color"));
    m_colorName.setToolTip(m_color);
    m_swatch.setToolTip(m_color);
    m_swatch.setAutoFillBackground(!m_color.isEmpty());
    if (!m_color.isEmpty()) {
      auto palette = m_swatch.palette();
      palette.setColor(QPalette::Window, QColor(m_color));
      m_swatch.setPalette(palette);
    }
  }

  QLineEdit m_name;
  QLabel m_swatch;
  QLabel m_colorName;
  QString m_color;
};

} // namespace

class ReopenDialog final : public QDialog {
public:
  ReopenDialog(Workspaces::Workspace workspace, WorkspaceUiPort &port,
               QWidget *parent)
      : QDialog(parent), m_workspace(std::move(workspace)), m_port(port) {
    setWindowTitle(tr("Reopen %1").arg(m_workspace.name));
    auto *layout = new QVBoxLayout(this);
    m_note.setObjectName(QStringLiteral("assignmentStatus"));
    m_note.setWordWrap(true);
    layout->addWidget(&m_note);
    m_form = new QFormLayout;
    layout->addLayout(m_form);
    auto *actions = new QHBoxLayout;
    auto *refreshButton = new QPushButton(tr("Refresh available windows"), this);
    refreshButton->setObjectName(QStringLiteral("refreshWindows"));
    actions->addWidget(refreshButton);
    actions->addStretch();
    m_restore = new QPushButton(tr("Restore workspace"), this);
    m_restore->setObjectName(QStringLiteral("restoreWorkspace"));
    actions->addWidget(m_restore);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName(QStringLiteral("cancelReopen"));
    actions->addWidget(cancel);
    layout->addLayout(actions);
    connect(refreshButton, &QPushButton::clicked, this, &ReopenDialog::refresh);
    connect(m_restore, &QPushButton::clicked, this, &ReopenDialog::restore);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    refresh();
  }

public:
  void reportLaunchFailure(const QString &desktopEntryId, const QString &message) {
    setResultText(message.isEmpty() ? tr("Could not launch %1.").arg(desktopEntryId)
                                    : message);
  }

private:
  void refresh() {
    QString error;
    const auto next = m_port.availableWindows(&error);
    if (!error.isEmpty()) {
      setResultText(error);
      return;
    }
    QSet<QString> usable;
    for (const auto &entry : next) {
      if (entry.window.eligible)
        usable.insert(entry.window.id);
    }
    for (auto it = m_explicit.begin(); it != m_explicit.end();) {
      if (!usable.contains(it.value()))
        it = m_explicit.erase(it);
      else
        ++it;
    }
    m_windows = next;
    rebuildRows();
  }

  void rebuildRows() {
    while (m_form->rowCount())
      m_form->removeRow(0);
    m_boxes.clear();
    const auto plan = Workspaces::assignWindows(m_workspace, available(), m_explicit);
    for (const auto &slot : m_workspace.applicationSlots) {
      auto *row = new QWidget(this);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins({});
      auto *choice = new QComboBox(row);
      choice->setObjectName(QStringLiteral("slot_") + slot.id);
      choice->addItem(tr("Choose a window"), QString());
      for (const auto &entry : m_windows) {
        if (entry.window.eligible) {
          choice->addItem(windowText(entry, m_port), entry.window.id);
          choice->setItemData(choice->count() - 1, entry.window.desktopEntryId,
                              Qt::ToolTipRole);
        }
      }
      choice->setCurrentIndex(choice->findData(plan.windowsBySlot.value(slot.id)));
      if (choice->currentIndex() < 0)
        choice->setCurrentIndex(0);
      rowLayout->addWidget(choice, 1);
      auto *launch = new QPushButton(tr("Launch application"), row);
      launch->setObjectName(QStringLiteral("launch_") + slot.id);
      const auto application = m_port.installedApplication(slot.desktopEntryId);
      launch->setEnabled(application.has_value());
      if (!application)
        launch->setToolTip(tr("Application unavailable: %1").arg(slot.desktopEntryId));
      connect(launch, &QPushButton::clicked, this, [this, slot] {
        QString error;
        if (!m_port.launchApplication(slot.desktopEntryId, slot.urls, &error))
          setResultText(error.isEmpty() ? tr("Could not launch %1.").arg(slot.label)
                                         : error);
        else
          setResultText(tr("Launch requested for %1. Refresh when its window appears.")
                            .arg(slot.label));
      });
      connect(choice, &QComboBox::currentIndexChanged, this,
              [this, slot, choice](int) {
                const auto id = choice->currentData().toString();
                if (id.isEmpty())
                  m_explicit.remove(slot.id);
                else
                  m_explicit.insert(slot.id, id);
                updatePlan();
              });
      rowLayout->addWidget(launch);
      auto *label = new QLabel(slot.label, this);
      label->setToolTip(tr("Desktop entry ID: %1").arg(slot.desktopEntryId));
      if (application)
        label->setText(QStringLiteral("%1 — %2").arg(slot.label, application->displayName));
      else
        label->setText(tr("%1 — application unavailable: %2")
                           .arg(slot.label, slot.desktopEntryId));
      m_form->addRow(label, row);
      m_boxes.insert(slot.id, choice);
    }
    updatePlan();
  }

  [[nodiscard]] QList<Workspaces::AvailableWindow> available() const {
    QList<Workspaces::AvailableWindow> result;
    for (const auto &entry : m_windows)
      result.append(entry.window);
    return result;
  }

  void updatePlan() {
    const auto plan = Workspaces::assignWindows(m_workspace, available(), m_explicit);
    // AGENT-GUARD: Reflect assignments returned by Workspaces without turning an
    // automatic match into a user choice; refresh must preserve explicit intent.
    for (const auto &slot : m_workspace.applicationSlots) {
      auto *const choice = m_boxes.value(slot.id);
      const auto matched = plan.windowsBySlot.value(slot.id);
      const auto index = matched.isEmpty() ? 0 : choice->findData(matched);
      const QSignalBlocker block(choice);
      choice->setCurrentIndex(index < 0 ? 0 : index);
    }
    m_restore->setEnabled(plan.complete());
    if (plan.complete())
      setResultText(tr("Every saved place has a distinct window."));
    else if (!plan.error.isEmpty())
      setResultText(plan.error);
    else
      setResultText(tr("Choose a distinct window for every saved place."));
  }

  void restore() {
    const auto plan = Workspaces::assignWindows(m_workspace, available(), m_explicit);
    if (!plan.complete()) {
      updatePlan();
      return;
    }
    QString error;
    const auto container = Workspaces::instantiate(
        m_workspace, plan, QUuid::createUuid().toString(QUuid::WithoutBraces), &error);
    if (!container || !m_port.restore(m_workspace, *container, &error)) {
      setResultText(error.isEmpty() ? tr("The workspace could not be restored.") : error);
      return;
    }
    setResultText(tr("Workspace restored."));
    accept();
  }

  void setResultText(const QString &text) { m_note.setText(text); }

  Workspaces::Workspace m_workspace;
  WorkspaceUiPort &m_port;
  QList<WorkspaceWindow> m_windows;
  QMap<QString, QString> m_explicit;
  QMap<QString, QComboBox *> m_boxes;
  QFormLayout *m_form = nullptr;
  QLabel m_note;
  QPushButton *m_restore = nullptr;
};

WorkspaceLibraryDialog::WorkspaceLibraryDialog(QString storageRoot,
                                               WorkspaceUiPort &port,
                                               QWidget *parent)
    : QDialog(parent), m_store(std::move(storageRoot)), m_port(port) {
  setWindowTitle(tr("Saved workspaces"));
  resize(420, 320);
  auto *layout = new QVBoxLayout(this);
  m_list = new QListWidget(this);
  m_list->setObjectName(QStringLiteral("savedWorkspaces"));
  layout->addWidget(m_list);
  m_result = new QLabel(this);
  m_result->setObjectName(QStringLiteral("workspaceResult"));
  m_result->setWordWrap(true);
  layout->addWidget(m_result);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto *save = buttons->addButton(tr("Save current"), QDialogButtonBox::ActionRole);
  auto *reopen = buttons->addButton(tr("Reopen"), QDialogButtonBox::ActionRole);
  save->setObjectName(QStringLiteral("saveCurrent"));
  reopen->setObjectName(QStringLiteral("reopenSelected"));
  layout->addWidget(buttons);
  connect(save, &QPushButton::clicked, this, &WorkspaceLibraryDialog::saveCurrent);
  connect(reopen, &QPushButton::clicked, this,
          &WorkspaceLibraryDialog::reopenSelected);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  reload();
}

void WorkspaceLibraryDialog::reload() {
  m_list->clear();
  int unreadable = 0;
  for (const auto &id : m_store.ids()) {
    QString error;
    const auto workspace = m_store.load(id, &error);
    if (!workspace) {
      ++unreadable;
      continue;
    }
    auto *item = new QListWidgetItem(workspace->name, m_list);
    item->setData(Qt::UserRole, workspace->id);
    if (!workspace->color.isEmpty()) {
      item->setIcon(swatchIcon(workspace->color));
      item->setToolTip(workspace->color);
    }
  }
  if (m_list->count())
    m_list->setCurrentRow(0);
  if (unreadable)
    m_result->setText(tr("%n saved workspace could not be read.", nullptr, unreadable));
}

void WorkspaceLibraryDialog::saveCurrent() {
  QString error;
  const auto current = m_port.currentContainer(&error);
  if (!current) {
    m_result->setText(error.isEmpty() ? tr("There is no complete container to save.")
                                      : error);
    return;
  }
  SaveDialog dialog(current->name, current->color, this);
  if (dialog.exec() != QDialog::Accepted)
    return;
  const auto saved = Workspaces::capture(
      current->workspaceId.isEmpty()
          ? QUuid::createUuid().toString(QUuid::WithoutBraces)
          : current->workspaceId,
      dialog.name(), dialog.color(),
      current->layout, current->applicationsByWindow, &error);
  if (!saved || !m_store.save(*saved, &error)) {
    m_result->setText(error.isEmpty() ? tr("The workspace could not be saved.") : error);
    return;
  }
  reload();
  m_result->setText(tr("Saved %1.").arg(saved->name));
}

void WorkspaceLibraryDialog::reopenSelected() {
  const auto *item = m_list->currentItem();
  if (!item) {
    m_result->setText(tr("Choose a saved workspace first."));
    return;
  }
  QString error;
  const auto workspace = m_store.load(item->data(Qt::UserRole).toString(), &error);
  if (!workspace) {
    m_result->setText(error.isEmpty() ? tr("The selected workspace is unavailable.")
                                      : error);
    return;
  }
  ReopenDialog dialog(*workspace, m_port, this);
  connect(this, &WorkspaceLibraryDialog::launchFailureReported, &dialog,
          &ReopenDialog::reportLaunchFailure);
  dialog.exec();
}

void WorkspaceLibraryDialog::reportLaunchFailure(QString desktopEntryId,
                                                  QString message) {
  const auto text = message.isEmpty() ? tr("Could not launch %1.").arg(desktopEntryId)
                                      : message;
  m_result->setText(text);
  emit launchFailureReported(std::move(desktopEntryId), std::move(message));
}

} // namespace QindaQt::WorkspacesUi
