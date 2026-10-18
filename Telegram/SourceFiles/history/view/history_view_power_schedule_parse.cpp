#include "history/view/history_view_power_schedule_parse.h"

#include "base/basic_types.h"
#include "ui/text/text_entity.h"

#include <QtCore/QDate>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <array>

namespace HistoryView {
namespace {

[[nodiscard]] QRegularExpression ScheduleExpression(const QString &pattern) {
	return QRegularExpression(
		pattern,
		QRegularExpression::CaseInsensitiveOption
			| QRegularExpression::UseUnicodePropertiesOption);
}

[[nodiscard]] bool IsScheduleValue(const QString &value) {
	static const auto none = ScheduleExpression(
		u"^(?:без\\h+відключень|відключень\\h+немає)$"_q);
	if (none.match(value).hasMatch()) {
		return true;
	}
	static const auto times = ScheduleExpression(
		u"^([0-9]{2}):([0-9]{2})\\h*[-–—]\\h*"
		"([0-9]{2}):([0-9]{2})$"_q);
	const auto intervals = value.split(QRegularExpression(u"\\h*[,;]\\h*"_q));
	for (const auto &interval : intervals) {
		const auto match = times.match(interval.trimmed());
		if (!match.hasMatch()) {
			return false;
		}
		const auto fromHour = match.captured(1).toInt();
		const auto fromMinute = match.captured(2).toInt();
		const auto tillHour = match.captured(3).toInt();
		const auto tillMinute = match.captured(4).toInt();
		const auto from = fromHour * 60 + fromMinute;
		const auto till = tillHour * 60 + tillMinute;
		if (fromHour > 23
			|| fromMinute > 59
			|| tillHour > 24
			|| tillMinute > 59
			|| (tillHour == 24 && tillMinute != 0)
			|| from >= till) {
			return false;
		}
	}
	return true;
}

} // namespace

bool IsPowerScheduleGroup(const QString &group) {
	static const auto expression = QRegularExpression(
		u"\\A[1-9][0-9]{0,2}\\.[1-9][0-9]?\\z"_q);
	return expression.match(group).hasMatch();
}

QString DefaultPowerScheduleFormat() {
	return u"Графік відключень ДТЕК на %day%, %date%. "
		"Група %group%: %time%."_q;
}

bool IsPowerScheduleFormat(const QString &format) {
	if (format.isEmpty()
		|| format.size() > 500
		|| format.contains('\n')
		|| format.contains('\r')
		|| format.contains(QChar::LineSeparator)
		|| format.contains(QChar::ParagraphSeparator)
		|| !format.contains(u"%time%"_q)) {
		return false;
	}
	auto remaining = format;
	static const auto fields = std::array{
		u"%day%"_q,
		u"%date%"_q,
		u"%group%"_q,
		u"%time%"_q,
	};
	for (const auto &field : fields) {
		remaining.remove(field);
	}
	return !remaining.contains('%');
}

PowerScheduleResult ParsePowerSchedule(
		const QString &text,
		const QString &group,
		const QString &format) {
	if (!IsPowerScheduleGroup(group)) {
		return {};
	}
	static const auto title = ScheduleExpression(
		u"\\bграфік\\h+відключень\\b"_q);
	static const auto date = QRegularExpression(
		u"\\b[0-9]{2}\\.[0-9]{2}\\.[0-9]{4}\\b"_q);
	static const auto row = ScheduleExpression(
		u"^\\h*Група\\h+([1-9][0-9]{0,2}\\.[1-9][0-9]?)"
		"\\h*:\\h*(.*?)\\h*$"_q);
	static const auto rowStart = ScheduleExpression(u"\\bГрупа\\b"_q);
	static const auto continuation = QRegularExpression(
		u"^[^\\p{L}0-9]*[0-9]{1,2}:[0-9]{2}"_q);

	auto selected = QString();
	auto firstRow = -1;
	auto groups = QSet<QString>();
	auto supported = true;
	for (auto start = 0; start < text.size();) {
		const auto newline = int(text.indexOf('\n', start));
		const auto end = (newline < 0) ? int(text.size()) : newline + 1;
		const auto line = text.mid(start, end - start).trimmed();
		const auto match = row.match(line);
		const auto looksLikeRow = rowStart.match(line).hasMatch();
		if (looksLikeRow && firstRow < 0) {
			firstRow = start;
		}
		const auto rowGroup = match.captured(1);
		const auto valid = !looksLikeRow
			|| (match.hasMatch()
				&& !groups.contains(rowGroup)
				&& IsScheduleValue(match.captured(2)));
		if (looksLikeRow) {
			supported = supported && valid;
			groups.insert(rowGroup);
			if (rowGroup == group) {
				selected = match.captured(2).simplified();
			}
		} else if (firstRow >= 0 && continuation.match(line).hasMatch()) {
			supported = false;
		}
		start = end;
	}
	if (firstRow < 0) {
		return {};
	}
	const auto header = text.left(firstRow);
	const auto dateMatch = date.match(header);
	const auto scheduleDate = QDate::fromString(
		dateMatch.captured(),
		u"dd.MM.yyyy"_q);
	if (!title.match(header).hasMatch()
		|| !dateMatch.hasMatch()
		|| !scheduleDate.isValid()) {
		return {};
	}
	if (!supported
		|| date.match(text, dateMatch.capturedEnd()).hasMatch()
		|| title.match(text, firstRow).hasMatch()) {
		return { PowerScheduleStatus::Unsupported };
	} else if (!groups.contains(group)) {
		return { PowerScheduleStatus::MissingGroup };
	}
	static const auto weekdays = std::array{
		u"понеділок"_q,
		u"вівторок"_q,
		u"середу"_q,
		u"четвер"_q,
		u"п’ятницю"_q,
		u"суботу"_q,
		u"неділю"_q,
	};
	auto compact = IsPowerScheduleFormat(format)
		? format
		: DefaultPowerScheduleFormat();
	compact.replace(u"%day%"_q, weekdays[scheduleDate.dayOfWeek() - 1])
		.replace(u"%date%"_q, dateMatch.captured())
		.replace(u"%group%"_q, group)
		.replace(u"%time%"_q, selected);
	return { PowerScheduleStatus::Filtered, compact };
}

TextWithEntities ApplyPowerSchedule(
		const TextWithEntities &original,
		const PowerScheduleResult &parsed) {
	if (parsed.status != PowerScheduleStatus::Filtered) {
		return original;
	}
	return { parsed.compact };
}

} // namespace HistoryView
