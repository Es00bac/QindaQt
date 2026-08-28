// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QtTest>

using namespace QindaQt::StatusNotifier;

namespace
{

OwnerKey validKey()
{
    OwnerKey key;
    key.uniqueName = QStringLiteral(":1.42");
    key.objectPath = QStringLiteral("/org/kde/StatusNotifierItem");
    key.generation = 1;
    return key;
}

Pixmap validPixmap(quint32 width = 4, quint32 height = 4)
{
    Pixmap pixmap;
    pixmap.width = width;
    pixmap.height = height;
    pixmap.argb = QByteArray(qsizetype(width) * qsizetype(height) * 4, Qt::Uninitialized);
    pixmap.argb.fill(char(0x7F));
    return pixmap;
}

IconPayload validIcon()
{
    IconPayload icon;
    icon.iconName = QStringLiteral("network-idle");
    icon.pixmaps = {validPixmap()};
    return icon;
}

ItemDescriptor validDescriptor()
{
    ItemDescriptor descriptor;
    descriptor.category = ItemCategory::Application;
    descriptor.identity = QStringLiteral("org.qindaqt.DemoTray");
    descriptor.title = QStringLiteral("Demo tray item");
    descriptor.status = ItemStatus::Active;
    descriptor.icon = validIcon();
    return descriptor;
}

MenuEntry menuItem(qsizetype parent, const QString &label)
{
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::Item;
    entry.parentId = parent;
    entry.label = label;
    return entry;
}

MenuEntry menuSeparator()
{
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::Separator;
    return entry;
}

MenuEntry menuSubMenu(qsizetype parent, const QString &label)
{
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::SubMenu;
    entry.parentId = parent;
    entry.label = label;
    return entry;
}

} // namespace

class StatusNotifierValuesTests final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidDescriptor()
    {
        const ValidationOutcome outcome = validateItemDescriptor(validDescriptor());
        QVERIFY(outcome.accepted);
        QCOMPARE(outcome.error, ValidationError::None);
    }

    void rejectsWellKnownNameAsOwner()
    {
        // The spoofed-owner defense starts here: a well-known bus name is
        // never a valid item owner because its ownership can change hands.
        OwnerKey key = validKey();
        key.uniqueName = QStringLiteral("org.example.StatusNotifier");
        const ValidationOutcome outcome = validateOwnerKey(key);
        QVERIFY(!outcome.accepted);
        QCOMPARE(outcome.error, ValidationError::InvalidOwnerName);
        QCOMPARE(outcome.reasonCode, QStringLiteral("owner-not-unique-bus-name"));
    }

    void rejectsMalformedOwnerKeys()
    {
        OwnerKey key = validKey();

        key.uniqueName = QStringLiteral(":1");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidOwnerName);
        key.uniqueName = QStringLiteral(":a.1");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidOwnerName);
        key.uniqueName = QStringLiteral(":1.");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidOwnerName);
        key.uniqueName = QStringLiteral(":.1");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidOwnerName);
        key.uniqueName = QString();
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidOwnerName);

        key = validKey();
        key.generation = 0;
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidGeneration);

        key = validKey();
        key.objectPath = QStringLiteral("relative/path");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidObjectPath);
        key.objectPath = QStringLiteral("/double//slash");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidObjectPath);
        key.objectPath = QStringLiteral("/trailing/");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidObjectPath);
        key.objectPath = QStringLiteral("/non#ascii");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidObjectPath);
        key.objectPath = QStringLiteral("/");
        QCOMPARE(validateOwnerKey(key).error, ValidationError::InvalidObjectPath);
    }

    void rejectsHostilePixmaps()
    {
        Pixmap pixmap = validPixmap();
        pixmap.width = 0;
        QCOMPARE(validatePixmap(pixmap).reasonCode, QStringLiteral("pixmap-zero-dimension"));

        pixmap = validPixmap();
        pixmap.height = 0;
        QCOMPARE(validatePixmap(pixmap).reasonCode, QStringLiteral("pixmap-zero-dimension"));

        pixmap = validPixmap(kMaxIconPixmapDimension + 1, 1);
        QCOMPARE(validatePixmap(pixmap).reasonCode, QStringLiteral("pixmap-dimension-exceeded"));

        pixmap = validPixmap();
        pixmap.argb.chop(1);
        QCOMPARE(validatePixmap(pixmap).reasonCode,
                 QStringLiteral("pixmap-byte-count-mismatch"));

        // A declared geometry whose byte count would exceed the icon budget
        // still fails closed on the byte-count rule.
        pixmap.width = kMaxIconPixmapDimension;
        pixmap.height = kMaxIconPixmapDimension;
        pixmap.argb = QByteArray(8, char(0));
        QCOMPARE(validatePixmap(pixmap).reasonCode,
                 QStringLiteral("pixmap-byte-count-mismatch"));

        // The exact budget-sized maximum pixmap is valid: the bounds line up.
        pixmap.argb = QByteArray(qsizetype(kMaxIconPixmapDimension)
                                     * qsizetype(kMaxIconPixmapDimension) * 4,
                                 char(0));
        QVERIFY(validatePixmap(pixmap).accepted);
    }

    void rejectsHostileIconPayloads()
    {
        IconPayload icon = validIcon();
        icon.iconName = QString(kMaxIconNameUtf8Bytes + 1, u'x');
        QCOMPARE(validateIconPayload(icon).reasonCode, QStringLiteral("unbounded-or-control-text"));

        icon = validIcon();
        icon.attentionMovieName = QString(1, QChar(0x0001));
        QCOMPARE(validateIconPayload(icon).reasonCode, QStringLiteral("unbounded-or-control-text"));

        icon = validIcon();
        while (icon.pixmaps.size() <= int(kMaxIconPixmaps)) {
            icon.pixmaps.append(validPixmap());
        }
        QCOMPARE(validateIconPayload(icon).reasonCode, QStringLiteral("pixmap-count-exceeded"));
    }

    void rejectsHostileToolTips()
    {
        ItemDescriptor descriptor = validDescriptor();
        descriptor.toolTip.title = QString(kMaxTitleUtf8Bytes + 1, u'x');
        QCOMPARE(validateToolTip(descriptor.toolTip).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        descriptor = validDescriptor();
        descriptor.toolTip.description = QStringLiteral("line\nbreak");
        QCOMPARE(validateToolTip(descriptor.toolTip).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        descriptor = validDescriptor();
        descriptor.toolTip.pixmaps = {validPixmap(), validPixmap(), validPixmap()};
        QCOMPARE(validateToolTip(descriptor.toolTip).reasonCode,
                 QStringLiteral("pixmap-count-exceeded"));
    }

    void acceptsValidFlatMenu()
    {
        MenuPayload menu;
        menu.entries = {
            menuItem(-1, QStringLiteral("Play")),
            menuSeparator(),
            menuSubMenu(-1, QStringLiteral("Outputs")),
            menuItem(2, QStringLiteral("Headphones")),
        };
        const ValidationOutcome outcome = validateMenu(menu);
        QVERIFY(outcome.accepted);
    }

    void rejectsHostileMenus()
    {
        MenuPayload menu;

        menu.entries = {menuItem(-1, QString())};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-item-unlabeled"));

        MenuEntry chattySeparator = menuSeparator();
        chattySeparator.label = QStringLiteral("not a separator");
        menu.entries = {chattySeparator};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("separator-with-content"));

        menu.entries = {menuSubMenu(-1, QString())};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("submenu-unlabeled"));

        menu.entries = {menuSubMenu(-1, QStringLiteral("Empty"))};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("submenu-empty"));

        // A child referencing a parent declared later is a hostile forward
        // reference and must fail closed.
        menu.entries = {menuItem(1, QStringLiteral("Orphan")),
                        menuSubMenu(-1, QStringLiteral("Parent"))};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-parent-invalid"));

        menu.entries = {menuItem(7, QStringLiteral("Orphan"))};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-parent-invalid"));

        // Children may nest only beneath submenus.
        menu.entries = {menuItem(-1, QStringLiteral("Plain item")),
                        menuItem(0, QStringLiteral("Child of an item"))};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-parent-not-submenu"));

        menu.entries = {menuSeparator(),
                        menuItem(0, QStringLiteral("Child of a separator"))};
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-parent-not-submenu"));

        // Depth beyond kMaxMenuDepth is rejected even when every entry is
        // individually well-formed.
        menu.entries.clear();
        for (int depth = 0; depth <= kMaxMenuDepth; ++depth) {
            menu.entries.append(menuSubMenu(depth == 0 ? qsizetype(-1) : depth - 1,
                                            QStringLiteral("Level %1").arg(depth)));
        }
        menu.entries.append(menuItem(kMaxMenuDepth, QStringLiteral("Too deep")));
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-depth-exceeded"));

        // The aggregate node budget is global across the whole payload.
        menu.entries.clear();
        for (qsizetype index = 0; index <= kMaxMenuNodes; ++index) {
            menu.entries.append(menuItem(-1, QStringLiteral("Entry %1").arg(index)));
        }
        QCOMPARE(validateMenu(menu).reasonCode, QStringLiteral("menu-node-budget-exceeded"));
    }

    void composedDescriptorGateRejectsHostileMenu()
    {
        // AGENT-GUARD: validateItemDescriptor is the single admission gate;
        // a hostile menu inside an otherwise valid descriptor must be caught
        // here and by the registry, never only by a direct validateMenu call.
        ItemDescriptor descriptor = validDescriptor();
        descriptor.menu.entries = {menuSubMenu(-1, QStringLiteral("Level 0")),
                                   menuSubMenu(0, QStringLiteral("Level 1")),
                                   menuSubMenu(1, QStringLiteral("Level 2")),
                                   menuSubMenu(2, QStringLiteral("Level 3")),
                                   menuItem(3, QStringLiteral("Too deep"))};
        const ValidationOutcome outcome = validateItemDescriptor(descriptor);
        QVERIFY(!outcome.accepted);
        QCOMPARE(outcome.reasonCode, QStringLiteral("menu-depth-exceeded"));

        descriptor = validDescriptor();
        descriptor.menu.entries = {menuItem(-1, QStringLiteral("Plain")),
                                   menuItem(0, QStringLiteral("Nested under item"))};
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode,
                 QStringLiteral("menu-parent-not-submenu"));
    }

    void rejectsUnknownCategoryAndStatus()
    {
        ItemDescriptor descriptor = validDescriptor();
        descriptor.category = static_cast<ItemCategory>(99);
        const ValidationOutcome categoryOutcome = validateItemDescriptor(descriptor);
        QVERIFY(!categoryOutcome.accepted);
        QCOMPARE(categoryOutcome.reasonCode, QStringLiteral("unknown-category"));

        descriptor = validDescriptor();
        descriptor.status = static_cast<ItemStatus>(7);
        const ValidationOutcome statusOutcome = validateItemDescriptor(descriptor);
        QVERIFY(!statusOutcome.accepted);
        QCOMPARE(statusOutcome.reasonCode, QStringLiteral("unknown-status"));
    }

    void rejectsUnboundedOrBlankIdentityAndTitle()
    {
        ItemDescriptor descriptor = validDescriptor();
        descriptor.identity.clear();
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode, QStringLiteral("invalid-identity"));

        descriptor.identity = QStringLiteral("   ");
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode, QStringLiteral("invalid-identity"));

        descriptor.identity = QString::fromUtf8("bad\0identity", 12);
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode, QStringLiteral("invalid-identity"));

        descriptor = validDescriptor();
        descriptor.identity = QString(kMaxIdentityUtf8Bytes + 1, u'x');
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode, QStringLiteral("invalid-identity"));

        descriptor = validDescriptor();
        descriptor.title = QString(QChar(0x001F));
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        descriptor = validDescriptor();
        descriptor.title = QStringLiteral(" \t ");
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        descriptor = validDescriptor();
        descriptor.title = QString(kMaxTitleUtf8Bytes + 1, u'x');
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        // An absent title stays valid: the identity is the accessibility
        // fallback and is locale-independent.
        descriptor = validDescriptor();
        descriptor.title.clear();
        QVERIFY(validateItemDescriptor(descriptor).accepted);
    }

    void rejectsControlCharactersIncludingC1()
    {
        // C1 controls are invisible on screen and must fail like C0.
        ItemDescriptor descriptor = validDescriptor();
        descriptor.toolTip.description = QString(QChar(0x0085));
        QCOMPARE(validateToolTip(descriptor.toolTip).reasonCode,
                 QStringLiteral("blank-or-control-text"));

        descriptor = validDescriptor();
        descriptor.identity = QString(QChar(0x009F));
        QCOMPARE(validateItemDescriptor(descriptor).reasonCode, QStringLiteral("invalid-identity"));

        QVERIFY(!isBoundedSafeText(QString(QChar(0x0080)), 16));
        QVERIFY(isBoundedSafeText(QStringLiteral("clean"), 16));
    }

    void boundedTextCountsUtf8BytesNotCodeUnits()
    {
        // U+00E9 is two UTF-8 bytes, so 128 code points are 256 bytes.
        const QString accents(128, QChar(0x00E9));
        QVERIFY(isBoundedSafeText(accents, 256));
        QVERIFY(!isBoundedSafeText(accents, 255));

        // A surrogate-free astral character occupies four bytes.
        const char32_t smileCodePoint = 0x1F642;
        const QString emoji = QString::fromUcs4(&smileCodePoint, 1);
        QCOMPARE(emoji.toUtf8().size(), 4);
        QVERIFY(isBoundedSafeText(emoji, 4));
        QVERIFY(!isBoundedSafeText(emoji, 3));
    }

    void uniqueBusNameSyntaxRules()
    {
        QVERIFY(isValidUniqueBusName(QStringLiteral(":1.42")));
        QVERIFY(isValidUniqueBusName(QStringLiteral(":10.1000")));
        QVERIFY(!isValidUniqueBusName(QStringLiteral(":1.42.43")));
        QVERIFY(!isValidUniqueBusName(QStringLiteral("1.42")));
        QVERIFY(!isValidUniqueBusName(QStringLiteral(":x.y")));
        QVERIFY(!isValidUniqueBusName(QStringLiteral("org.example.Service")));
        QVERIFY(!isValidUniqueBusName(QStringLiteral(":1.42extra")));
    }
};

QTEST_GUILESS_MAIN(StatusNotifierValuesTests)
#include "tst_status_notifier_values.moc"
