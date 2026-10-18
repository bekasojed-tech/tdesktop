#include "history/view/history_view_power_schedule.h"

#include "history/view/history_view_power_schedule_parse.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_filter.h"
#include "lang/lang_keys.h"
#include "ui/text/text_utilities.h"

namespace HistoryView {

QString PowerScheduleGroup(not_null<PeerData*> peer) {
	const auto group = HistoryFilter::Options(peer).scheduleGroup;
	return IsPowerScheduleGroup(group) ? group : QString();
}

std::optional<TextWithEntities> PowerScheduleText(not_null<HistoryItem*> item) {
	if (!item->isPost() || item->richPage()) {
		return std::nullopt;
	}
	const auto options = HistoryFilter::Options(item->history()->peer);
	const auto &group = options.scheduleGroup;
	if (group.isEmpty() || !item->computeUnavailableReason().isEmpty()) {
		return std::nullopt;
	}
	const auto &original = item->originalText();
	const auto parsed = ParsePowerSchedule(
		original.text,
		group,
		options.scheduleFormat);
	if (parsed.status == PowerScheduleStatus::NotSchedule) {
		return std::nullopt;
	}
	auto result = ApplyPowerSchedule(original, parsed);
	if (parsed.status != PowerScheduleStatus::Filtered) {
		result.append(u"\n\n"_q).append(tr::italic(
			(parsed.status == PowerScheduleStatus::MissingGroup)
				? tr::lng_power_schedule_missing(tr::now, lt_group, group)
				: tr::lng_power_schedule_unsupported(tr::now)));
	}
	return result;
}

} // namespace HistoryView
