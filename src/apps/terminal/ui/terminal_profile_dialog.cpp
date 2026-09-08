// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_profile_dialog.h"

#include "session/terminal_launch_policy.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include "qindaqt/controls/application_icon.h"
#include <QVBoxLayout>

#include <algorithm>

namespace QindaQt::Apps::Terminal {
namespace {

constexpr auto kBuiltinRole = Qt::UserRole;

QString profileLabel(const TerminalProfile &profile,
                     const QString &defaultProfileId) {
  QString label = profile.name;
  if (profile.id == defaultProfileId) {
    label += QStringLiteral(" (default)");
  }
  return label;
}

} // namespace

TerminalProfileDialog::TerminalProfileDialog(
    const QList<TerminalProfile> &userProfiles, const QString &defaultProfileId,
    bool restoreTabs, const QStringList &themeIds, QWidget *parent)
    : QDialog(parent), m_userProfiles(userProfiles),
      m_defaultProfileId(defaultProfileId), m_restoreTabs(restoreTabs) {
  setObjectName(QStringLiteral("terminalProfileDialog"));
  setAttribute(Qt::WA_WindowPropagation, true);
  setWindowTitle(QStringLiteral("Terminal Profiles"));
  setAccessibleName(QStringLiteral("Terminal profile settings"));
  resize(720, 480);
  buildUi(themeIds);
  refreshList();
  if (m_list->count() > 0) {
    m_list->setCurrentRow(0);
  }
}

void TerminalProfileDialog::buildUi(const QStringList &themeIds) {
  auto *splitter = new QSplitter(this);
  m_editingSurface = splitter;

  auto *listSide = new QWidget(splitter);
  auto *listLayout = new QVBoxLayout(listSide);
  listLayout->setContentsMargins(0, 0, 0, 0);
  m_list = new QListWidget(listSide);
  m_list->setObjectName(QStringLiteral("terminalProfileList"));
  m_list->setAccessibleName(QStringLiteral("Profiles"));
  m_list->setSpacing(4);
  m_list->setMinimumWidth(190);
  listLayout->addWidget(m_list);
  auto *listButtons = new QHBoxLayout();
  auto *addButton = new QPushButton(QStringLiteral("Add"), listSide);
  addButton->setIcon(QindaQt::Controls::applicationIcon(QStringLiteral("list-add")));
  addButton->setObjectName(QStringLiteral("profileAddButton"));
  auto *removeButton = new QPushButton(QStringLiteral("Remove"), listSide);
  removeButton->setIcon(QindaQt::Controls::applicationIcon(QStringLiteral("list-remove")));
  removeButton->setObjectName(QStringLiteral("profileRemoveButton"));
  auto *defaultButton =
      new QPushButton(QStringLiteral("Set as Default"), listSide);
  defaultButton->setObjectName(QStringLiteral("profileDefaultButton"));
  listButtons->addWidget(addButton);
  listButtons->addWidget(removeButton);
  listLayout->addWidget(defaultButton);
  listButtons->addStretch(1);
  listLayout->addLayout(listButtons);
  splitter->addWidget(listSide);

  auto *formSide = new QWidget(splitter);
  auto *formLayout = new QFormLayout(formSide);
  formLayout->setContentsMargins(16, 8, 0, 8);
  formLayout->setVerticalSpacing(12);
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_name = new QLineEdit(formSide);
  m_name->setObjectName(QStringLiteral("profileNameEdit"));
  m_name->setAccessibleName(QStringLiteral("Profile name"));
  m_shellProgram = new QLineEdit(formSide);
  m_shellProgram->setObjectName(QStringLiteral("profileShellEdit"));
  m_shellProgram->setAccessibleName(QStringLiteral("Shell program"));
  m_shellProgram->setPlaceholderText(
      QStringLiteral("Inherit the default shell"));
  m_shellArguments = new QPlainTextEdit(formSide);
  m_shellArguments->setObjectName(QStringLiteral("profileShellArgsEdit"));
  m_shellArguments->setAccessibleName(
      QStringLiteral("Shell arguments, one per line"));
  m_shellArguments->setPlaceholderText(
      QStringLiteral("One argument per line"));
  m_shellArguments->setMaximumBlockCount(TerminalLaunchPolicy::kMaxArguments +
                                         1);
  m_shellArguments->setMaximumHeight(90);
  m_fontFamily = new QLineEdit(formSide);
  m_fontFamily->setObjectName(QStringLiteral("profileFontFamilyEdit"));
  m_fontFamily->setAccessibleName(QStringLiteral("Font family"));
  m_fontFamily->setPlaceholderText(QStringLiteral("Theme default"));
  m_fontSize = new QSpinBox(formSide);
  m_fontSize->setObjectName(QStringLiteral("profileFontSizeSpin"));
  m_fontSize->setAccessibleName(QStringLiteral("Font size"));
  m_fontSize->setRange(0, TerminalProfile::kMaxFontSize);
  m_fontSize->setSpecialValueText(QStringLiteral("Theme default"));
  m_colorScheme = new QComboBox(formSide);
  m_colorScheme->setObjectName(QStringLiteral("profileColorSchemeCombo"));
  m_colorScheme->setAccessibleName(QStringLiteral("Color scheme"));
  m_colorScheme->addItems(themeIds);
  m_scrollback = new QSpinBox(formSide);
  m_scrollback->setObjectName(QStringLiteral("profileScrollbackSpin"));
  m_scrollback->setAccessibleName(QStringLiteral("Scrollback lines"));
  m_scrollback->setRange(0, TerminalProfile::kMaxScrollbackLines);
  m_bellPolicy = new QComboBox(formSide);
  m_bellPolicy->setObjectName(QStringLiteral("profileBellCombo"));
  m_bellPolicy->setAccessibleName(QStringLiteral("Bell policy"));
  m_bellPolicy->addItem(QStringLiteral("Silent"));
  m_bellPolicy->addItem(QStringLiteral("Audible"));
  formLayout->addRow(QStringLiteral("Name"), m_name);
  formLayout->addRow(QStringLiteral("Shell program"), m_shellProgram);
  formLayout->addRow(QStringLiteral("Shell arguments"), m_shellArguments);
  formLayout->addRow(QStringLiteral("Font family"), m_fontFamily);
  formLayout->addRow(QStringLiteral("Font size"), m_fontSize);
  formLayout->addRow(QStringLiteral("Color scheme"), m_colorScheme);
  formLayout->addRow(QStringLiteral("Scrollback lines"), m_scrollback);
  formLayout->addRow(QStringLiteral("Bell"), m_bellPolicy);
  splitter->addWidget(formSide);
  splitter->setStretchFactor(1, 1);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(splitter);
  m_applyStatus = new QLabel(this);
  m_applyStatus->setObjectName(QStringLiteral("profileApplyStatus"));
  m_applyStatus->setAccessibleName(QStringLiteral("Profile save status"));
  m_applyStatus->setWordWrap(true);
  m_applyStatus->hide();
  layout->addWidget(m_applyStatus);
  m_buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(m_buttons, &QDialogButtonBox::accepted, this,
          &TerminalProfileDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this,
          &TerminalProfileDialog::reject);
  layout->addWidget(m_buttons);

  connect(m_list, &QListWidget::currentRowChanged, this,
          [this](int) { loadSelectedIntoFields(); });
  const auto writeBack = [this] {
    if (!m_loadingFields) {
      writeFieldsToSelected();
    }
  };
  connect(m_name, &QLineEdit::textEdited, this, writeBack);
  connect(m_shellProgram, &QLineEdit::textEdited, this, writeBack);
  connect(m_shellArguments, &QPlainTextEdit::textChanged, this, writeBack);
  connect(m_fontFamily, &QLineEdit::textEdited, this, writeBack);
  connect(m_fontSize, &QSpinBox::valueChanged, this, writeBack);
  connect(m_colorScheme, &QComboBox::currentIndexChanged, this, writeBack);
  connect(m_scrollback, &QSpinBox::valueChanged, this, writeBack);
  connect(m_bellPolicy, &QComboBox::currentIndexChanged, this, writeBack);
  connect(addButton, &QPushButton::clicked, this, [this] {
    writeFieldsToSelected();
    if (m_userProfiles.size() >= TerminalProfile::kMaxUserProfiles) {
      return;
    }
    TerminalProfile profile = builtinDefaultProfile();
    profile.id = generateProfileId();
    profile.name = QStringLiteral("Profile %1").arg(m_userProfiles.size() + 1);
    m_userProfiles.append(profile);
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
  });
  connect(removeButton, &QPushButton::clicked, this, [this] {
    TerminalProfile *profile = selectedProfile();
    if (profile == nullptr || selectedIsBuiltin()) {
      return;
    }
    if (m_defaultProfileId == profile->id) {
      m_defaultProfileId = builtinDefaultProfileId();
    }
    m_userProfiles.removeAt(m_list->currentRow() - 1);
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
  });
  connect(defaultButton, &QPushButton::clicked, this, [this] {
    TerminalProfile *profile = selectedProfile();
    if (profile == nullptr) {
      return;
    }
    writeFieldsToSelected();
    m_defaultProfileId = profile->id;
    refreshList();
  });
}

bool TerminalProfileDialog::selectedIsBuiltin() const {
  const auto *item = m_list->currentItem();
  return item != nullptr && item->data(kBuiltinRole).toBool();
}

TerminalProfile *TerminalProfileDialog::selectedProfile() {
  const int row = m_list->currentRow();
  if (selectedIsBuiltin()) {
    return nullptr;
  }
  // Row 0 is the built-in entry; user profiles follow in m_userProfiles
  // order, kept in sync by refreshList().
  const int index = row - 1;
  return index >= 0 && index < m_userProfiles.size() ? &m_userProfiles[index]
                                                     : nullptr;
}

void TerminalProfileDialog::refreshList() {
  const int previousRow = m_list->currentRow();
  m_list->clear();
  auto *builtinItem = new QListWidgetItem(
      profileLabel(builtinDefaultProfile(), m_defaultProfileId), m_list);
  builtinItem->setIcon(QindaQt::Controls::applicationIcon(QStringLiteral("utilities-terminal")));
  builtinItem->setData(kBuiltinRole, true);
  for (int index = 0; index < m_userProfiles.size(); ++index) {
    auto *item = new QListWidgetItem(
        profileLabel(m_userProfiles.at(index), m_defaultProfileId), m_list);
    item->setIcon(QindaQt::Controls::applicationIcon(QStringLiteral("utilities-terminal")));
    item->setData(kBuiltinRole, false);
  }
  if (m_list->count() > 0) {
    m_list->setCurrentRow(qBound(0, previousRow, m_list->count() - 1));
  }
}

void TerminalProfileDialog::loadSelectedIntoFields() {
  m_loadingFields = true;
  const TerminalProfile *profile = selectedProfile();
  const TerminalProfile &effective =
      profile != nullptr ? *profile : builtinDefaultProfile();
  m_name->setText(effective.name);
  m_name->setEnabled(profile != nullptr);
  m_shellProgram->setText(effective.shellProgram);
  m_shellProgram->setEnabled(profile != nullptr);
  m_shellArguments->setPlainText(
      effective.shellArguments.join(QLatin1Char('\n')));
  m_shellArguments->setEnabled(profile != nullptr);
  m_fontFamily->setText(effective.fontFamily);
  m_fontFamily->setEnabled(profile != nullptr);
  m_fontSize->setValue(effective.fontSize);
  m_fontSize->setEnabled(profile != nullptr);
  const int schemeIndex = m_colorScheme->findText(effective.colorSchemeId);
  m_colorScheme->setCurrentIndex(qMax(0, schemeIndex));
  m_colorScheme->setEnabled(profile != nullptr);
  m_scrollback->setValue(effective.scrollbackLines);
  m_scrollback->setEnabled(profile != nullptr);
  m_bellPolicy->setCurrentIndex(
      effective.bellPolicy == TerminalProfile::BellPolicy::Audible ? 1 : 0);
  m_bellPolicy->setEnabled(profile != nullptr);
  m_loadingFields = false;
}

void TerminalProfileDialog::writeFieldsToSelected() {
  TerminalProfile *profile = selectedProfile();
  if (profile == nullptr) {
    return;
  }
  profile->name = m_name->text();
  profile->shellProgram = m_shellProgram->text();
  const QString editedArguments = m_shellArguments->toPlainText();
  const QString originalArguments =
      profile->shellArguments.join(QLatin1Char('\n'));
  if (editedArguments != originalArguments) {
    profile->shellArguments =
        editedArguments.isEmpty()
            ? QStringList{}
            : editedArguments.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
  }
  profile->fontFamily = m_fontFamily->text();
  profile->fontSize = m_fontSize->value();
  profile->colorSchemeId = m_colorScheme->currentText();
  profile->scrollbackLines = m_scrollback->value();
  profile->bellPolicy = m_bellPolicy->currentIndex() == 1
                            ? TerminalProfile::BellPolicy::Audible
                            : TerminalProfile::BellPolicy::Silent;
  // Keep the visible label in sync (name/default changes show immediately).
  if (auto *item = m_list->currentItem()) {
    item->setText(profileLabel(*profile, m_defaultProfileId));
  }
}

void TerminalProfileDialog::accept() {
  if (m_applyInFlight) {
    return;
  }
  writeFieldsToSelected();
  QStringList problems;
  if (m_userProfiles.size() > TerminalProfile::kMaxUserProfiles) {
    problems.append(QStringLiteral("At most %1 user profiles are allowed")
                        .arg(TerminalProfile::kMaxUserProfiles));
  }
  for (const TerminalProfile &profile : std::as_const(m_userProfiles)) {
    const ProfileValidation validation = validateTerminalProfile(profile);
    if (!validation.ok) {
      problems.append(QStringLiteral("%1: %2").arg(
          profile.name.isEmpty() ? profile.id : profile.name,
          validation.diagnostic));
    }
  }
  const bool defaultKnown =
      m_defaultProfileId == builtinDefaultProfileId() ||
      std::any_of(m_userProfiles.begin(), m_userProfiles.end(),
                  [this](const TerminalProfile &profile) {
                    return profile.id == m_defaultProfileId;
                  });
  if (!defaultKnown) {
    problems.append(QStringLiteral("The default profile is not in the "
                                   "list"));
  }
  if (!problems.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("Invalid profiles"),
        QStringLiteral("Fix the following before saving:\n• %1")
            .arg(problems.join(QStringLiteral("\n• "))));
    return;
  }
  m_applyInFlight = true;
  m_editingSurface->setEnabled(false);
  m_buttons->setEnabled(false);
  m_applyStatus->setText(QStringLiteral("Saving terminal profiles…"));
  m_applyStatus->setAccessibleDescription(m_applyStatus->text());
  m_applyStatus->show();
  emit applyRequested();
}

void TerminalProfileDialog::finishApply(bool allApplied,
                                        const QString &accessibleStatus) {
  if (!m_applyInFlight) {
    return;
  }
  m_applyStatus->setText(accessibleStatus.left(1024));
  m_applyStatus->setAccessibleDescription(m_applyStatus->text());
  m_applyStatus->show();
  m_applyInFlight = false;
  if (allApplied) {
    QDialog::accept();
    return;
  }
  m_editingSurface->setEnabled(true);
  m_buttons->setEnabled(true);
}

} // namespace QindaQt::Apps::Terminal
