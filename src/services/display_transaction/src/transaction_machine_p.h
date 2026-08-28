// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_transaction/transaction_types.h>

#include <limits>

namespace QindaQt::DisplayTransaction::Private
{

inline bool validTransactionId(const QString &transactionId)
{
    return !transactionId.isEmpty()
        && Display::isBoundedText(transactionId, Display::kMaxTransactionIdUtf8Bytes);
}

inline bool activeState(const MachineState state)
{
    return state != MachineState::Discovering && state != MachineState::Ready;
}

inline bool rollbackInProgress(const MachineState state)
{
    return state == MachineState::SettlingTopology
        || state == MachineState::RevertingApply
        || state == MachineState::RevertingObserve
        || state == MachineState::RevertBackoff;
}

inline quint64 saturatedDeadline(const quint64 now, const quint64 duration)
{
    return duration > std::numeric_limits<quint64>::max() - now
        ? std::numeric_limits<quint64>::max()
        : now + duration;
}

} // namespace QindaQt::DisplayTransaction::Private
