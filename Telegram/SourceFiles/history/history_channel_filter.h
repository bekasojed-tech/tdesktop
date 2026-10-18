#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>
#include <vector>

namespace HistoryFilter {

struct ChannelOptions {
	bool hideButtonPosts = false;
	bool hidePaidPosts = false;
	QString scheduleGroup;
	QString scheduleFormat;

	friend bool operator==(
		const ChannelOptions &,
		const ChannelOptions &) = default;
};

[[nodiscard]] bool HidesPosts(const ChannelOptions &options);
[[nodiscard]] QByteArray SerializeOptions(const ChannelOptions &options);
[[nodiscard]] std::optional<ChannelOptions> DeserializeOptions(
	const QByteArray &data);
[[nodiscard]] ChannelOptions MigrateOptions(
	const std::vector<QString> &usernames,
	const QString &legacyGroup);

} // namespace HistoryFilter
