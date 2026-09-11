// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>

#include <qindaqt/apps/settings_input/config_change_announcement.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>
#include <QRegularExpression>

namespace QindaQt::Apps::SettingsInput {
namespace {

Q_LOGGING_CATEGORY(lcLayoutPort, "qindaqt.settings.input.layouts", QtInfoMsg)

constexpr auto KWinService = "org.kde.KWin";

// AGENT-CONTRACT: KWin applies LayoutList/VariantList from kxkbrc together
// with Use=true; a private KWin started with that group applied the list,
// and a later write took effect after the ConfigChanged announcement
// (ADR-0134).
constexpr auto UseKey = "Use";
constexpr auto LayoutListKey = "LayoutList";
constexpr auto VariantListKey = "VariantList";
constexpr qsizetype MaximumLayouts = 32;

// AGENT-CONTRACT: xkb identifiers are lowercase ASCII letters, digits and
// underscore (see /usr/share/X11/xkb/rules/evdev.xml). Everything else is
// rejected before it can reach kxkbrc, because KWin feeds these values
// straight into libxkbcommon rules lookups.
const QRegularExpression &validCodePattern() {
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z0-9_]{1,32}$"));
    return pattern;
}

bool desktopAuthorityPresent(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(KWinService));
}

} // namespace

bool KeyboardLayoutSelection::isValidIdentifier() const noexcept {
    const auto valid = [](const QString &code) {
        if (code.isEmpty()) {
            return true; // an empty variant is the layout default
        }
        return validCodePattern().match(code).hasMatch();
    };
    return validCodePattern().match(layout).hasMatch() && valid(variant);
}

KeyboardLayoutPort::~KeyboardLayoutPort() = default;

QtKeyboardLayoutPort::QtKeyboardLayoutPort(QString kxkbrcPath,
                                           QDBusConnection bus)
    : m_kxkbrcPath(std::move(kxkbrcPath)), m_bus(std::move(bus)) {}

QList<KeyboardLayoutSelection>
QtKeyboardLayoutPort::configuredLayouts(QString *error) const {
    const KConfigGroup group(
        KSharedConfig::openConfig(m_kxkbrcPath, KConfig::SimpleConfig),
        QStringLiteral("Layout"));
    const QStringList layoutCodes =
        group.readEntry(LayoutListKey, QStringList{});
    QStringList variantCodes =
        group.readEntry(VariantListKey, QStringList{});
    while (variantCodes.size() < layoutCodes.size()) {
        variantCodes.append(QString());
    }
    QList<KeyboardLayoutSelection> layouts;
    for (qsizetype index = 0; index < layoutCodes.size(); ++index) {
        KeyboardLayoutSelection row;
        row.layout = layoutCodes.at(index);
        row.variant = index < variantCodes.size() ? variantCodes.at(index)
                                                  : QString();
        // AGENT-GUARD: Total decode — a row that is not a valid identifier
        // is dropped whole, never partially trusted into the UI or written
        // back on the next store.
        if (!row.isValidIdentifier()) {
            qCInfo(lcLayoutPort,
                   "dropping malformed layout row '%s'/'%s' from %s",
                   qPrintable(row.layout), qPrintable(row.variant),
                   qPrintable(m_kxkbrcPath));
            continue;
        }
        layouts.append(row);
    }
    Q_UNUSED(error);
    return layouts;
}

StoreResult QtKeyboardLayoutPort::writeConfiguredLayouts(
    const QList<KeyboardLayoutSelection> &layouts, QString *error) const {
    // AGENT-GUARD: kxkbrc with an empty or invalid layout list makes the
    // desktop's keyboard layout machinery fall back unpredictably; the route
    // must keep at least one valid layout configured.
    if (layouts.isEmpty() || layouts.size() > MaximumLayouts) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "The layout list must hold between 1 and %1 layouts")
                         .arg(MaximumLayouts);
        }
        return StoreResult::Failed;
    }
    QStringList codes;
    QStringList variants;
    QStringList seen;
    for (const KeyboardLayoutSelection &row : layouts) {
        if (!row.isValidIdentifier() || seen.contains(row.layout)) {
            if (error != nullptr) {
                *error = QStringLiteral("Invalid or duplicate layout '%1'")
                             .arg(row.layout);
            }
            return StoreResult::Failed;
        }
        seen.append(row.layout);
        codes.append(row.layout);
        variants.append(row.variant);
    }
    const KSharedConfigPtr configFile = KSharedConfig::openConfig(
        m_kxkbrcPath, KConfig::SimpleConfig);
    KConfigGroup group(configFile, QStringLiteral("Layout"));
    group.writeEntry(UseKey, true);
    group.writeEntry(LayoutListKey, codes);
    group.writeEntry(VariantListKey, variants);
    if (!group.sync()) {
        if (error != nullptr) {
            *error =
                QStringLiteral("Could not write %1").arg(m_kxkbrcPath);
        }
        return StoreResult::Failed;
    }
    QHash<QString, QByteArrayList> changes;
    changes.insert(QStringLiteral("Layout"),
                   QByteArrayList{QByteArray(UseKey),
                                  QByteArray(LayoutListKey),
                                  QByteArray(VariantListKey)});
    if (!announceConfigChange(m_bus, m_kxkbrcPath, changes) ||
        !desktopAuthorityPresent(m_bus)) {
        qCInfo(lcLayoutPort,
               "layout list stored, but no running desktop was told; "
               "it applies on the next session");
        return StoreResult::StoredButReloadFailed;
    }
    return StoreResult::Stored;
}

} // namespace QindaQt::Apps::SettingsInput
