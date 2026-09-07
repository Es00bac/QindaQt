// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/containerappearance.h"

#include <QRegularExpression>

namespace QindaQt::Compositor {
namespace {

bool safePresentationText(const QString &value)
{
  if (value.contains(QChar::Null)) {
    return false;
  }
  for (const QChar character : value) {
    if (character.category() == QChar::Other_Control
        || character.category() == QChar::Other_Format) {
      return false;
    }
  }
  return true;
}

} // namespace

QString normalizedContainerName(const QString &rawName)
{
  const QString trimmed = rawName.trimmed();
  if (trimmed.isEmpty() || trimmed.size() > ContainerNameMaximumCharacters
      || !safePresentationText(trimmed)) {
    return {};
  }
  return trimmed;
}

bool isValidContainerColor(const QString &colorHex)
{
  static const QRegularExpression pattern(QStringLiteral("^#[0-9A-F]{6}$"));
  return pattern.match(colorHex).hasMatch();
}

QString normalizedContainerColor(const QString &rawColor)
{
  const QString upper = rawColor.trimmed().toUpper();
  return isValidContainerColor(upper) ? upper : QString{};
}

QVector<ContainerColorSwatch> containerColorSwatches()
{
  // AGENT-NOTE: A fixed, curated palette rather than a free-form color picker.
  // No production QWidget color-picker dialog exists in this tree, and a
  // small named set keeps the group-menu submenu, keyboard traversal, and any
  // later dock legend deterministic. Values chosen for contrast against both
  // light and dark ChromePalette surfaces.
  return {
      {QStringLiteral("#E5484D"), QStringLiteral("Red")},
      {QStringLiteral("#F76B15"), QStringLiteral("Orange")},
      {QStringLiteral("#FFB224"), QStringLiteral("Yellow")},
      {QStringLiteral("#30A46C"), QStringLiteral("Green")},
      {QStringLiteral("#0091FF"), QStringLiteral("Blue")},
      {QStringLiteral("#8E4EC6"), QStringLiteral("Purple")},
      {QStringLiteral("#D6409F"), QStringLiteral("Pink")},
      {QStringLiteral("#8B8D98"), QStringLiteral("Gray")},
  };
}

} // namespace QindaQt::Compositor
