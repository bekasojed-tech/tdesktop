#include "history/history_channel_filter.h"

#include "base/basic_types.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace HistoryFilter {

bool HidesPosts(const ChannelOptions &options) {
	return options.hideButtonPosts || options.hidePaidPosts;
}

QByteArray SerializeOptions(const ChannelOptions &options) {
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"hideButtonPosts"_q, options.hideButtonPosts },
		{ u"hidePaidPosts"_q, options.hidePaidPosts },
		{ u"scheduleGroup"_q, options.scheduleGroup },
		{ u"scheduleFormat"_q, options.scheduleFormat },
	}).toJson(QJsonDocument::Compact);
}

std::optional<ChannelOptions> DeserializeOptions(const QByteArray &data) {
	if (data.size() > 4096) {
		return std::nullopt;
	}
	const auto document = QJsonDocument::fromJson(data);
	if (!document.isObject()) {
		return std::nullopt;
	}
	const auto object = document.object();
	const auto hide = object.value(u"hideButtonPosts"_q);
	const auto paid = object.value(u"hidePaidPosts"_q);
	const auto group = object.value(u"scheduleGroup"_q);
	const auto format = object.value(u"scheduleFormat"_q);
	if (object.value(u"version"_q) != QJsonValue(1)
		|| (!hide.isUndefined() && !hide.isBool())
		|| (!paid.isUndefined() && !paid.isBool())
		|| (!group.isUndefined() && !group.isString())
		|| (!format.isUndefined() && !format.isString())
		|| group.toString().size() > 6
		|| format.toString().size() > 500) {
		return std::nullopt;
	}
	return ChannelOptions{
		.hideButtonPosts = hide.toBool(),
		.hidePaidPosts = paid.toBool(),
		.scheduleGroup = group.toString(),
		.scheduleFormat = format.toString(),
	};
}

ChannelOptions MigrateOptions(
		const std::vector<QString> &usernames,
		const QString &legacyGroup) {
	auto result = ChannelOptions{ .scheduleGroup = legacyGroup };
	for (const auto &username : usernames) {
		if (username.compare(u"XTUBKA"_q, Qt::CaseInsensitive) == 0) {
			result.hideButtonPosts = true;
			break;
		}
	}
	return result;
}

} // namespace HistoryFilter
