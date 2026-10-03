// SPDX-License-Identifier: LGPL-3.0-or-later
#include "shortcut_wire.h"
#include <QDBusMetaType>
#include <QJsonArray>
#include <QMap>
#include <QSet>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &wire, const PortalShortcut &shortcut) {
  wire.beginStructure();
  wire << shortcut.id << shortcut.options;
  wire.endStructure();
  return wire;
}
const QDBusArgument &operator>>(const QDBusArgument &wire,
                                PortalShortcut &shortcut) {
  wire.beginStructure();
  wire >> shortcut.id >> shortcut.options;
  wire.endStructure();
  return wire;
}
void registerShortcutWire() {
  static const bool once = [] {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcuts>();
    return true;
  }();
  Q_UNUSED(once);
}
namespace {
QKeySequence preferred(const QString &trigger) {
  // Adapt the existing portal's XDG modifier grammar using public Qt keys.
  // A preferred trigger is advisory: unsupported keys start unassigned in
  // the real editor, rather than silently acquiring the wrong combination.
  if (trigger.size() > 128)
    return {};
  auto pieces = trigger.split('+');
  if (pieces.isEmpty())
    return {};
  const QMap<QString, QString> modifiers{{"CTRL", "Ctrl"},
                                         {"ALT", "Alt"},
                                         {"SHIFT", "Shift"},
                                         {"LOGO", "Meta"},
                                         {"NUM", "Num"}};
  for (int i = 0; i < pieces.size() - 1; ++i) {
    if (!modifiers.contains(pieces[i]))
      return {};
    pieces[i] = modifiers.value(pieces[i]);
  }
  // AGENT-CONTRACT: portal clients use XKB XF86 names, while Qt's portable
  // key-sequence parser uses human media-key names. Without this translation
  // Gabbee's default dictation key appears in consent as unassigned.
  const QMap<QString, QString> mediaKeys{
      {"XF86AudioPrev", "Media Previous"},
      {"XF86AudioNext", "Media Next"},
      {"XF86AudioStop", "Media Stop"},
      {"XF86AudioPlay", "Media Play"},
      {"XF86AudioPause", "Media Pause"},
      {"XF86AudioRecord", "Media Record"},
      {"XF86AudioRaiseVolume", "Volume Up"},
      {"XF86AudioLowerVolume", "Volume Down"},
      {"XF86AudioMute", "Volume Mute"}};
  if (pieces.last() == QLatin1String("ISO_Left_Tab"))
    pieces.last() = QStringLiteral("Backtab");
  else if (mediaKeys.contains(pieces.last()))
    pieces.last() = mediaKeys.value(pieces.last());
  const QKeySequence key(pieces.join('+'), QKeySequence::PortableText);
  if (key.count() != 1 || key[0].key() == Qt::Key_unknown)
    return {};
  return key;
}
} // namespace
std::optional<ShortcutDrafts> shortcutDrafts(const PortalShortcuts &offered) {
  if (offered.size() > 32)
    return {};
  ShortcutDrafts drafts;
  QSet<QString> ids;
  for (const auto &shortcut : offered) {
    if (shortcut.id.isEmpty() || shortcut.id.size() > 128 ||
        shortcut.id.contains(QChar(0x1f)))
      return {};
    if (ids.contains(shortcut.id))
      continue;
    ids.insert(shortcut.id);
    const auto description = shortcut.options.value("description");
    if (description.metaType() != QMetaType::fromType<QString>() ||
        description.toString().isEmpty() || description.toString().size() > 512)
      return {};
    const auto trigger = shortcut.options.value("preferred_trigger");
    if (trigger.isValid() &&
        trigger.metaType() != QMetaType::fromType<QString>())
      return {};
    drafts.append(
        {shortcut.id, description.toString(), preferred(trigger.toString())});
  }
  return drafts;
}
QJsonObject shortcutFrame(const QString &app, const QString &parent,
                          const QString &component,
                          const ShortcutDrafts &drafts) {
  QJsonArray rows;
  for (const auto &draft : drafts)
    rows.append(
        QJsonObject{{"id", draft.id},
                    {"description", draft.description},
                    {"key", draft.key.toString(QKeySequence::PortableText)}});
  return {{"type", "shortcuts"},
          {"app", app},
          {"parent", parent},
          {"component", component},
          {"shortcuts", rows}};
}
std::optional<ShortcutDrafts>
shortcutDraftsFromFrame(const QJsonObject &frame) {
  if (frame.size() != 5 || frame.value("type") != QJsonValue("shortcuts") ||
      !frame.value("app").isString() || !frame.value("parent").isString() ||
      frame.value("parent").toString().size() > 2048 ||
      !frame.value("component").isString() ||
      !frame.value("shortcuts").isArray())
    return {};
  const auto rows = frame.value("shortcuts").toArray();
  if (rows.size() > 32)
    return {};
  ShortcutDrafts drafts;
  QSet<QString> ids;
  for (const auto &value : rows) {
    const auto row = value.toObject();
    const auto id = row.value("id"), description = row.value("description"),
               text = row.value("key");
    if (row.size() != 3 || !id.isString() || id.toString().isEmpty() ||
        id.toString().size() > 128 || id.toString().contains(QChar(0x1f)) ||
        ids.contains(id.toString()) || !description.isString() ||
        description.toString().isEmpty() ||
        description.toString().size() > 512 || !text.isString() ||
        text.toString().size() > 256)
      return {};
    const QKeySequence key(text.toString(), QKeySequence::PortableText);
    if (key.toString(QKeySequence::PortableText) != text.toString() ||
        (!key.isEmpty() && key[0].key() == Qt::Key_unknown))
      return {};
    ids.insert(id.toString());
    drafts.append({id.toString(), description.toString(), key});
  }
  return drafts;
}
std::optional<ShortcutDrafts> shortcutSelection(const ShortcutDrafts &offered,
                                                const QJsonObject &results) {
  if (results.size() != 1 || !results.value("shortcuts").isArray())
    return {};
  QJsonObject frame = shortcutFrame({}, {}, {}, {});
  frame["shortcuts"] = results.value("shortcuts");
  const auto selected = shortcutDraftsFromFrame(frame);
  if (!selected || selected->size() != offered.size())
    return {};
  for (int i = 0; i < offered.size(); ++i)
    if (selected->at(i).id != offered[i].id ||
        selected->at(i).description != offered[i].description)
      return {};
  return selected;
}
PortalShortcuts shortcutDescriptions(const ShortcutDrafts &drafts) {
  PortalShortcuts rows;
  for (const auto &draft : drafts)
    rows.append({draft.id,
                 {{"description", draft.description},
                  {"trigger_description",
                   draft.key.toString(QKeySequence::NativeText)}}});
  return rows;
}
} // namespace QindaQt::Services::Portal
