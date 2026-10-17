#include "history/view/history_view_power_schedule.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "history/view/history_view_power_schedule_parse.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/text/text_utilities.h"

namespace HistoryView {
namespace {

[[nodiscard]] std::string PreferenceKey(not_null<PeerData*> peer) {
	return "power-schedule/"
		+ std::to_string(peer->session().userId().bare)
		+ "/"
		+ std::to_string(peer->id.value);
}

} // namespace

QString PowerScheduleGroup(not_null<PeerData*> peer) {
	if (!peer->isBroadcast()) {
		return {};
	}
	const auto group = QString::fromUtf8(
		Core::App().settings().readPref<QByteArray>(PreferenceKey(peer)));
	return IsPowerScheduleGroup(group) ? group : QString();
}

void SetPowerScheduleGroup(not_null<PeerData*> peer, const QString &group) {
	Expects(group.isEmpty() || IsPowerScheduleGroup(group));

	if (PowerScheduleGroup(peer) == group) {
		return;
	}
	Core::App().settings().writePref<QByteArray>(
		PreferenceKey(peer),
		group.toUtf8());
	peer->owner().requestPeerTextRefresh(peer);
}

std::optional<TextWithEntities> PowerScheduleText(not_null<HistoryItem*> item) {
	if (!item->isPost() || item->richPage()) {
		return std::nullopt;
	}
	const auto group = PowerScheduleGroup(item->history()->peer);
	if (group.isEmpty() || !item->computeUnavailableReason().isEmpty()) {
		return std::nullopt;
	}
	const auto &original = item->originalText();
	const auto parsed = ParsePowerSchedule(original.text, group);
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
