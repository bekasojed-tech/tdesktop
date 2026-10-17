#pragma once

#include "ui/text/text_entity.h"

#include <optional>

class HistoryItem;
class PeerData;

namespace HistoryView {

[[nodiscard]] QString PowerScheduleGroup(not_null<PeerData*> peer);
void SetPowerScheduleGroup(not_null<PeerData*> peer, const QString &group);
[[nodiscard]] std::optional<TextWithEntities> PowerScheduleText(
	not_null<HistoryItem*> item);

} // namespace HistoryView
