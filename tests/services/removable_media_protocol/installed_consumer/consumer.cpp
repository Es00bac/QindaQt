// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <qindaqt/services/removable_media_protocol/media_limits.h>
using namespace QindaQt::RemovableMedia;
int main() {
  Snapshot original;
  const auto encoded = encodeSnapshot(original);
  if (!encoded.succeeded() || encoded.payload.size() > kMaxSnapshotBytes) return 1;
  Snapshot decoded;
  if (!decodeSnapshot(encoded.payload, decoded).succeeded() || decoded != original) return 2;
  decoded.availability = Availability::Ready;
  if (validateSnapshot(decoded).accepted()) return 3;
  const Snapshot retained = decoded;
  if (decodeSnapshot(encoded.payload.left(8), decoded).succeeded() || decoded != retained) return 4;
  return 0;
}
