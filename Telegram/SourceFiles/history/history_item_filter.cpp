#include "history/history_item_filter.h"

#include "base/flat_map.h"
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

namespace HistoryFilter {
namespace {

[[nodiscard]] std::string PreferenceSuffix(not_null<PeerData*> peer) {
	return std::to_string(peer->session().userId().bare)
		+ "/"
		+ std::to_string(peer->id.value);
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

ChannelOptions Options(not_null<PeerData*> peer) {
	if (!peer->isBroadcast()) {
		return {};
	}
	const auto suffix = PreferenceSuffix(peer);
	const auto key = "channel-filters/" + suffix;
	auto &settings = Core::App().settings();
	const auto raw = settings.readPref<QByteArray>(key);
	static auto cache = base::flat_map<
		std::string,
		std::pair<QByteArray, ChannelOptions>>();
	const auto cached = cache.find(key);
	if (cached != end(cache) && cached->second.first == raw) {
		return cached->second.second;
	}
	auto result = ChannelOptions();
	auto stored = raw;
	if (!raw.isEmpty()) {
		result = DeserializeOptions(raw).value_or(ChannelOptions());
	} else {
		const auto legacyGroup = QString::fromUtf8(
			settings.readPref<QByteArray>("power-schedule/" + suffix));
		result = MigrateOptions(peer->usernames(), legacyGroup);
		if (result != ChannelOptions()) {
			stored = SerializeOptions(result);
			settings.writePref<QByteArray>(key, stored);
		}
	}
	if (!HistoryView::IsPowerScheduleGroup(result.scheduleGroup)) {
		result.scheduleGroup.clear();
	}
	if (!HistoryView::IsPowerScheduleFormat(result.scheduleFormat)) {
		result.scheduleFormat.clear();
	}
	if (!stored.isEmpty()) {
		cache[key] = { stored, result };
	}
	return result;
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
	if (previous != options) {
		peer->owner().history(peer)->refreshLocalFilters();
	}
}

bool IsHidden(not_null<const HistoryItem*> item) {
	if (!item->isPost()) {
		return false;
	}
	const auto options = Options(item->history()->peer);
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
