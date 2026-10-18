#include "history/view/history_view_power_schedule_parse.h"

#include "base/basic_types.h"
#include "crl/crl_on_main.h"
#include "rpl/rpl.h"
#include "ui/text/text_utilities.h"

#include <QtCore/QCoreApplication>

#include <array>
#include <iostream>

namespace {

using namespace HistoryView;

auto FailedChecks = 0;
auto TotalChecks = 0;

void Check(bool passed, const char *name) {
	++TotalChecks;
	if (!passed) {
		++FailedChecks;
		std::cout << "FAILED: " << name << std::endl;
	}
}

[[nodiscard]] TextWithEntities Apply(
		const TextWithEntities &text,
		const QString &group) {
	const auto parsed = ParsePowerSchedule(text.text, group);
	return ApplyPowerSchedule(text, parsed);
}

} // namespace

namespace crl {

rpl::producer<> on_main_update_requests() {
	return rpl::never<>();
}

} // namespace crl

int main(int argc, char *argv[]) {
	const auto app = QCoreApplication(argc, argv);
	const auto header = u"Запланований графік відключень ДТЕК\n"
		"на неділю, 18.10.2026\n\n"_q;
	const auto first = u"Група 1.1: 22:00-24:00\n"_q;
	const auto chosen = u"Група 12.1: 07:00-10:30, 22:00–24:00\n"_q;
	const auto last = u"Група 12.2: 10:00-13:30\n"_q;
	const auto footer = u"\nУвага! Можливі зміни.\nhttps://example.org"_q;
	const auto source = header + first + chosen + last + footer;
	const auto group = u"12.1"_q;
	const auto compact = u"Графік відключень ДТЕК на неділю, 18.10.2026. "
		"Група 12.1: 07:00-10:30, 22:00–24:00."_q;
	Check(ParsePowerSchedule(source, group).status
		== PowerScheduleStatus::Filtered, "recognizes the dated schedule");
	Check(Apply({ source }, group).text == compact,
		"formats the schedule as one compact sentence without a footer");
	Check(ParsePowerSchedule(header + first + last, group).status
		== PowerScheduleStatus::MissingGroup,
		"missing group is not interpreted as no outages");
	Check(Apply({ header + first + last }, group).text == header + first + last,
		"missing group retains the complete original");
	Check(Apply({ source }, u"1.1"_q).text
		== u"Графік відключень ДТЕК на неділю, 18.10.2026. "
			"Група 1.1: 22:00-24:00."_q,
		"matches the exact group, not a suffix");
	Check(Apply({ source }, u"12.2"_q).text
		== u"Графік відключень ДТЕК на неділю, 18.10.2026. "
			"Група 12.2: 10:00-13:30."_q,
		"can select the last row");
	Check(Apply({ source }, group).text
		!= Apply({ QString(source).replace(u"07:00"_q, u"08:00"_q) }, group).text,
		"edits are reflected without a stale text cache");
	Check(ParsePowerSchedule(header + first + chosen + chosen, group).status
		== PowerScheduleStatus::Unsupported, "duplicate group retains original");
	Check(ParsePowerSchedule(source + u"\n19.10.2026"_q, group).status
		== PowerScheduleStatus::Unsupported, "multiple dates retain original");
	Check(ParsePowerSchedule(source + u"\n"_q + header, group).status
		== PowerScheduleStatus::Unsupported, "multiple schedules retain original");
	Check(ParsePowerSchedule(u"Інші новини\n"_q + chosen, group).status
		== PowerScheduleStatus::NotSchedule, "ordinary posts stay unchanged");
	Check(ParsePowerSchedule(header, group).status
		== PowerScheduleStatus::NotSchedule, "no rows stays unchanged");
	Check(ParsePowerSchedule(QString(source).replace(
		u"18.10.2026"_q, u"31.02.2026"_q), group).status
		== PowerScheduleStatus::NotSchedule, "invalid date stays unchanged");
	Check(ParsePowerSchedule(source, QString()).status
		== PowerScheduleStatus::NotSchedule, "disabled filter stays unchanged");
	Check(ParsePowerSchedule(header + first + u"Група 12.1: ?"_q, group).status
		== PowerScheduleStatus::Unsupported, "unknown values retain original");
	Check(ParsePowerSchedule(header + chosen + u"Група 2: 01:00-02:00"_q,
		group).status == PowerScheduleStatus::Unsupported,
		"partially recognized group lists retain original");
	Check(ParsePowerSchedule(header + u"Група 12.1: 25:00-26:00"_q,
		group).status == PowerScheduleStatus::Unsupported,
		"invalid hours retain original");
	Check(ParsePowerSchedule(header + u"Група 12.1: 22:00-24:01"_q,
		group).status == PowerScheduleStatus::Unsupported,
		"invalid midnight retains original");
	Check(ParsePowerSchedule(header + u"Група 12.1: 12:00-10:00"_q,
		group).status == PowerScheduleStatus::Unsupported,
		"reversed intervals retain original");
	Check(ParsePowerSchedule(header + u"Група 12.1: без відключень"_q,
		group).status == PowerScheduleStatus::Filtered,
		"explicit no-outage statement is recognized");
	Check(ParsePowerSchedule(source + u"\n22:00-24:00"_q, group).status
		== PowerScheduleStatus::Unsupported,
		"multiline intervals retain the original instead of misattributing times");
	Check(ParsePowerSchedule(header + chosen + u"⚡ Група 1.1: 07:00-08:00"_q,
		group).status == PowerScheduleStatus::Unsupported,
		"unrecognized row prefixes retain the original");
	Check(Apply({ QString(source).replace(u"\n"_q, u"\r\n"_q) }, group).text
		== compact,
		"CRLF posts produce the same single-line sentence");
	Check(Apply({ QString(source).replace(u"Група "_q, u"Група\u00A0"_q) }, group).text
		== compact,
		"nonbreaking spaces are recognized");
	Check(IsPowerScheduleGroup(group)
		&& !IsPowerScheduleGroup(u"12.1\n"_q)
		&& !IsPowerScheduleGroup(u"12.1 extra"_q)
		&& !IsPowerScheduleGroup(u"0.1"_q), "group validation is exact");

	auto marked = TextWithEntities{ source };
	marked.entities.push_back({ EntityType::CustomUrl, 13, 17,
		u"https://www.dtek-kem.com.ua/ua/shutdowns"_q });
	const auto selectedOffset = int(header.size() + first.size());
	marked.entities.push_back({ EntityType::Bold, selectedOffset, 10 });
	marked.entities.push_back({ EntityType::Italic,
		int(header.size() + first.size() + chosen.size()), int(last.size()) });
	const auto filtered = Apply(marked, group);
	Check(filtered.text == compact && filtered.entities.empty(),
		"compact text has no hyperlinks or original formatting");
	Check(!filtered.text.contains('\n')
		&& !filtered.text.contains(u"Local filter"_q)
		&& !filtered.text.contains(u"https://"_q),
		"compact text contains only the requested single-line content");
	const auto fallback = Apply(marked, u"99.1"_q);
	Check(fallback.text == marked.text
		&& fallback.entities.size() == marked.entities.size(),
		"fallback preserves the complete original and its entities");
	Check(Apply({ header + first + u"Група 12.1: 19:00-22:30"_q }, group).text
		== u"Графік відключень ДТЕК на неділю, 18.10.2026. "
			"Група 12.1: 19:00-22:30."_q,
		"matches the requested example exactly");
	const auto weekdays = std::array{
		u"понеділок"_q,
		u"вівторок"_q,
		u"середу"_q,
		u"четвер"_q,
		u"п’ятницю"_q,
		u"суботу"_q,
		u"неділю"_q,
	};
	for (auto day = 0; day != int(weekdays.size()); ++day) {
		const auto date = u"%1.10.2026"_q.arg(19 + day);
		const auto post = QString(source).replace(u"18.10.2026"_q, date);
		const auto expected = u"Графік відключень ДТЕК на %1, %2. "
			"Група 12.1: 07:00-10:30, 22:00–24:00."_q.arg(weekdays[day], date);
		Check(Apply({ post }, group).text == expected,
			"uses the Ukrainian weekday matching the schedule date");
	}
	Check(IsPowerScheduleFormat(DefaultPowerScheduleFormat()),
		"the default template is valid");
	Check(IsPowerScheduleFormat(u"%group%: %time%"_q),
		"a user can omit optional template fields");
	Check(!IsPowerScheduleFormat(u"%date%"_q),
		"a template must include the actual outage times");
	Check(!IsPowerScheduleFormat(u"%time% %unknown%"_q),
		"unknown fields are rejected");
	Check(!IsPowerScheduleFormat(u"%time%\n%date%"_q),
		"multiline templates are rejected");
	Check(!IsPowerScheduleFormat(QString(501, QChar('x')) + u"%time%"_q),
		"oversized templates are rejected");
	Check(ParsePowerSchedule(source, group, u"%date% / %group%: %time%"_q).compact
		== u"18.10.2026 / 12.1: 07:00-10:30, 22:00–24:00"_q,
		"uses the saved custom template");
	Check(ParsePowerSchedule(source, group, u"%time% %time%"_q).compact
		== u"07:00-10:30, 22:00–24:00 07:00-10:30, 22:00–24:00"_q,
		"repeated fields are filled consistently");
	Check(ParsePowerSchedule(source, group, u"invalid"_q).compact == compact,
		"invalid stored templates fall back to the default");
	Check(ParsePowerSchedule(header + first, group, u"%time%"_q).status
		== PowerScheduleStatus::MissingGroup,
		"a custom template never turns a missing group into an outage schedule");
	Check(marked.text == source && marked.entities.size() == 3,
		"filtering does not mutate the source message");

	std::cout << (TotalChecks - FailedChecks) << "/" << TotalChecks
		<< " checks passed." << std::endl;
	return FailedChecks ? 1 : 0;
}
