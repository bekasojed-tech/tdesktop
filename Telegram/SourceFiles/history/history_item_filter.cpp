#include "history/history_item_filter.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_groups.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/view/history_view_power_schedule_parse.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"

#include <map>

namespace HistoryFilter {
namespace {

[[nodiscard]] std::string PreferenceSuffix(not_null<PeerData*> peer) {
	return std::to_string(peer->session().userId().bare)
		+ "/"
		+ std::to_string(peer->id.value);
}

using OptionsCacheKey = std::pair<uint64, uint64>;

[[nodiscard]] OptionsCacheKey CacheKey(not_null<PeerData*> peer) {
	return { peer->session().userId().bare, peer->id.value };
}

// std::map keeps references returned by Options() valid on insertion.
[[nodiscard]] std::map<OptionsCacheKey, ChannelOptions> &Cache() {
	static auto result = std::map<OptionsCacheKey, ChannelOptions>();
	return result;
}

[[nodiscard]] bool HasButtons(not_null<const HistoryItem*> item) {
	if (const auto markup = item->inlineReplyMarkup()) {
		for (const auto &row : markup->data.rows) {
			if (!row.empty()) {
				return true;
			}
		}
	}
	return false;
}

[[nodiscard]] bool HasUnpaidMedia(not_null<const HistoryItem*> item) {
	const auto media = item->media();
	const auto invoice = media ? media->invoice() : nullptr;
	return invoice && invoice->isPaidMedia && Data::HasUnpaidMedia(*invoice);
}

[[nodiscard]] bool Matches(
		not_null<const HistoryItem*> item,
		const ChannelOptions &options) {
	return (options.hideButtonPosts && HasButtons(item))
		|| (options.hidePaidPosts && HasUnpaidMedia(item));
}

} // namespace

const ChannelOptions &Options(not_null<PeerData*> peer) {
	static const auto empty = ChannelOptions();
	if (!peer->isBroadcast()) {
		return empty;
	}
	const auto cacheKey = CacheKey(peer);
	auto &cache = Cache();
	if (const auto i = cache.find(cacheKey); i != end(cache)) {
		return i->second;
	}
	const auto suffix = PreferenceSuffix(peer);
	const auto key = "channel-filters/" + suffix;
	auto &settings = Core::App().settings();
	const auto raw = settings.readPref<QByteArray>(key);
	auto result = ChannelOptions();
	if (!raw.isEmpty()) {
		result = DeserializeOptions(raw).value_or(ChannelOptions());
	} else {
		const auto legacyGroup = QString::fromUtf8(
			settings.readPref<QByteArray>("power-schedule/" + suffix));
		result = MigrateOptions(peer->usernames(), legacyGroup);
		if (result != ChannelOptions()) {
			settings.writePref<QByteArray>(key, SerializeOptions(result));
		} else if (!peer->isLoaded()) {
			// Usernames may still arrive, keep the migration retryable.
			return empty;
		}
	}
	if (!HistoryView::IsPowerScheduleGroup(result.scheduleGroup)) {
		result.scheduleGroup.clear();
	}
	if (!HistoryView::IsPowerScheduleFormat(result.scheduleFormat)) {
		result.scheduleFormat.clear();
	}
	return cache.emplace(cacheKey, std::move(result)).first->second;
}

void SetOptions(not_null<PeerData*> peer, const ChannelOptions &options) {
	Expects(peer->isBroadcast());
	Expects(options.scheduleGroup.isEmpty()
		|| HistoryView::IsPowerScheduleGroup(options.scheduleGroup));
	Expects(options.scheduleFormat.isEmpty()
		|| HistoryView::IsPowerScheduleFormat(options.scheduleFormat));

	const auto previous = Options(peer);
	Core::App().settings().writePref<QByteArray>(
		"channel-filters/" + PreferenceSuffix(peer),
		SerializeOptions(options));
	Cache().erase(CacheKey(peer));
	if (previous != options) {
		peer->owner().history(peer)->refreshLocalFilters();
	}
}

bool IsHidden(not_null<const HistoryItem*> item) {
	if (!item->isPost()) {
		return false;
	}
	const auto &options = Options(item->history()->peer);
	if (!HidesPosts(options)) {
		return false;
	} else if (Matches(item, options)) {
		return true;
	} else if (const auto group = item->history()->owner().groups().find(item)) {
		for (const auto &part : group->items) {
			if (Matches(part, options)) {
				return true;
			}
		}
	}
	return false;
}

} // namespace HistoryFilter
