#pragma once

#include <QtCore/QString>

struct TextWithEntities;

namespace HistoryView {

enum class PowerScheduleStatus {
	NotSchedule,
	Filtered,
	MissingGroup,
	Unsupported,
};

struct PowerScheduleResult {
	PowerScheduleStatus status = PowerScheduleStatus::NotSchedule;
	QString compact;
};

[[nodiscard]] bool IsPowerScheduleGroup(const QString &group);
[[nodiscard]] PowerScheduleResult ParsePowerSchedule(
	const QString &text,
	const QString &group);
[[nodiscard]] TextWithEntities ApplyPowerSchedule(
	const TextWithEntities &original,
	const PowerScheduleResult &parsed);

} // namespace HistoryView
