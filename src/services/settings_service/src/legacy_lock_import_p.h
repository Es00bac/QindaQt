// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
namespace QindaQt::Services::SettingsService {
class SettingsRepository;
namespace Private {
void applyLegacyLockImport(SettingsRepository &repository,
                           const QString &readOnlyPath);
}
} // namespace QindaQt::Services::SettingsService
