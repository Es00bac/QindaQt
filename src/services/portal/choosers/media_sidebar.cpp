// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_sidebar.h"
#include "media_presenter.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
namespace Media = QindaQt::RemovableMedia;
ChooserMediaSidebar::ChooserMediaSidebar(ChooserMediaPresenter &presenter, QWidget *parent)
    : QWidget(parent), m_presenter(presenter), m_devices(new QListWidget(this)), m_notice(new QLabel(this)),
      m_open(new QPushButton(tr("Open device"), this)), m_readOnly(new QPushButton(tr("Mount read-only"), this)),
      m_unmount(new QPushButton(tr("Unmount"), this)), m_remove(new QPushButton(tr("Safely remove"), this)),
      m_details(new QPushButton(tr("Details"), this)), m_recovery(new QPushButton(this)),
      m_owner(new QPushButton(tr("Open Removable Media"), this)) {
    setObjectName(QStringLiteral("portalMediaSidebar")); setMaximumWidth(240);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("Devices"), this));
    m_devices->setObjectName(QStringLiteral("portalMediaDevices"));
    m_devices->setAccessibleName(tr("Removable devices and partitions")); layout->addWidget(m_devices, 1);
    m_notice->setTextFormat(Qt::PlainText); m_notice->setWordWrap(true);
    m_notice->setObjectName(QStringLiteral("portalMediaNotice")); layout->addWidget(m_notice);
    const QList<QPushButton *> buttons{m_open, m_readOnly, m_unmount, m_remove, m_details, m_recovery, m_owner};
    const QStringList names{QStringLiteral("portalMediaOpen"), QStringLiteral("portalMediaReadOnly"),
        QStringLiteral("portalMediaUnmount"), QStringLiteral("portalMediaRemove"), QStringLiteral("portalMediaDetails"),
        QStringLiteral("portalMediaRecovery"), QStringLiteral("portalMediaOwner")};
    for (qsizetype i = 0; i < buttons.size(); ++i) { buttons[i]->setObjectName(names[i]); layout->addWidget(buttons[i]); }
    connect(m_devices, &QListWidget::currentItemChanged, this, [this] { update(); });
    connect(m_devices, &QListWidget::itemActivated, this, [this] { m_presenter.open(handle()); });
    connect(m_open, &QPushButton::clicked, this, [this] { m_presenter.open(handle()); });
    connect(m_readOnly, &QPushButton::clicked, this, [this] { m_presenter.request(handle(), Media::Action::MountReadOnly, true); });
    connect(m_unmount, &QPushButton::clicked, this, [this] { m_presenter.request(handle(), Media::Action::Unmount); });
    connect(m_remove, &QPushButton::clicked, this, [this] { m_presenter.request(handle(), Media::Action::Remove); });
    connect(m_details, &QPushButton::clicked, this, [this] { m_presenter.request(handle(), Media::Action::ShowDetails); });
    connect(m_recovery, &QPushButton::clicked, &presenter, &ChooserMediaPresenter::recover);
    connect(m_owner, &QPushButton::clicked, &presenter, &ChooserMediaPresenter::openOwner);
    connect(&presenter, &ChooserMediaPresenter::changed, this, [this] { populate(); });
    populate();
}
void ChooserMediaSidebar::populate() {
    const auto selected = handle();
    // A replacement inventory cannot implicitly open or retain an old handle.
    const QSignalBlocker blocker(m_devices); m_devices->clear();
    for (const auto &row : m_presenter.snapshot().rows) {
        QString label = row.displayName;
        if (row.partitionNumber) label += tr(" · partition %1").arg(row.partitionNumber);
        label += row.locked ? tr("\nLocked") : row.mountState != Media::MountState::Mounted
            ? tr("\nNot mounted") : row.readOnly == Media::ReadOnlyState::ReadOnly ? tr("\nMounted read-only") : tr("\nMounted");
        auto *item = new QListWidgetItem(label, m_devices);
        item->setData(Qt::UserRole, row.attachment.handle); item->setToolTip(row.kind);
        if (row.attachment.handle == selected) m_devices->setCurrentItem(item);
    }
    update();
}
QString ChooserMediaSidebar::handle() const {
    const auto *item = m_devices->currentItem(); return item ? item->data(Qt::UserRole).toString() : QString{};
}
void ChooserMediaSidebar::update() {
    const auto snapshot = m_presenter.snapshot(); const Media::VolumeRow *selected = nullptr;
    for (const auto &row : snapshot.rows) if (row.attachment.handle == handle()) { selected = &row; break; }
    const bool available = selected && snapshot.availability == Media::Availability::Ready && !m_presenter.busy();
    m_open->setEnabled(available && (selected->actions.open.enabled || selected->actions.mount.enabled));
    m_readOnly->setEnabled(available && selected->actions.mountReadOnly.enabled);
    m_unmount->setEnabled(available && selected->actions.unmount.enabled);
    m_remove->setEnabled(available && selected->actions.remove.enabled);
    m_details->setEnabled(available && selected->actions.showDetails.enabled);
    m_notice->setText(m_presenter.notice()); m_recovery->setVisible(snapshot.availability != Media::Availability::Ready);
    m_recovery->setEnabled(snapshot.availability != Media::Availability::Loading);
    m_recovery->setText(m_presenter.recoveryLabel()); m_owner->setVisible(snapshot.availability != Media::Availability::Ready);
    for (auto *button : {m_open, m_readOnly, m_unmount, m_remove, m_details}) {
        button->setToolTip(available ? tr("Uses current device information; locked-device changes belong to Removable Media.")
                                    : tr("Choose an available device. Locked or busy devices require Removable Media."));
        button->setAccessibleDescription(button->toolTip());
    }
}
