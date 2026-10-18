#pragma once

#include "history/history_channel_filter.h"

class HistoryItem;
class PeerData;

namespace HistoryFilter {

[[nodiscard]] ChannelOptions Options(not_null<PeerData*> peer);
void SetOptions(not_null<PeerData*> peer, const ChannelOptions &options);
[[nodiscard]] bool IsHidden(not_null<const HistoryItem*> item);

} // namespace HistoryFilter
