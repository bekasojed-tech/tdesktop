#include "history/history_channel_filter.h"

#include "base/basic_types.h"

#include <QtCore/QCoreApplication>

#include <iostream>

namespace {

auto FailedChecks = 0;
auto TotalChecks = 0;

void Check(bool passed, const char *name) {
	++TotalChecks;
	if (!passed) {
		++FailedChecks;
		std::cout << "FAILED: " << name << std::endl;
	}
}

} // namespace

int main(int argc, char *argv[]) {
	const auto app = QCoreApplication(argc, argv);
	using namespace HistoryFilter;
	const auto options = ChannelOptions{
		.hideButtonPosts = true,
		.hidePaidPosts = true,
		.scheduleGroup = u"12.1"_q,
		.scheduleFormat = u"%date% — %group%: %time%"_q,
	};
	Check(DeserializeOptions(SerializeOptions(options)) == options,
		"all settings survive a storage round trip");
	const auto disabled = ChannelOptions();
	Check(DeserializeOptions(SerializeOptions(disabled)) == disabled,
		"explicitly disabled filters survive a storage round trip");
	Check(DeserializeOptions("{\"version\":1}") == disabled,
		"missing optional fields default to disabled");
	Check(!DeserializeOptions(QByteArray()), "empty storage is not a saved rule");
	Check(!DeserializeOptions("[]"), "rejects a non-object document");
	Check(!DeserializeOptions("broken"), "rejects malformed storage");
	Check(!DeserializeOptions("{\"version\":2}"), "preserves unknown future versions");
	Check(!DeserializeOptions("{\"hideButtonPosts\":true}"),
		"requires an explicit schema version");
	Check(!DeserializeOptions("{\"version\":1,\"hideButtonPosts\":\"false\"}"),
		"does not treat a string as an enabled flag");
	Check(!DeserializeOptions("{\"version\":1,\"hidePaidPosts\":1}"),
		"does not treat a number as the paid posts flag");
	Check(DeserializeOptions("{\"version\":1,\"hidePaidPosts\":true}")
			== ChannelOptions{ .hidePaidPosts = true },
		"reads the paid posts flag on its own");
	Check(HidesPosts({ .hidePaidPosts = true })
			&& HidesPosts({ .hideButtonPosts = true })
			&& !HidesPosts({ .scheduleGroup = u"12.1"_q }),
		"either hiding rule enables post hiding");
	Check(!DeserializeOptions("{\"version\":1,\"scheduleGroup\":12.1}"),
		"does not accept a numeric group");
	Check(!DeserializeOptions("{\"version\":1,\"scheduleFormat\":false}"),
		"does not accept a boolean template");
	Check(!DeserializeOptions(QByteArray(4097, ' ')), "bounds stored document size");
	Check(!DeserializeOptions(SerializeOptions({
		.scheduleFormat = QString(501, QChar('x')),
	})), "bounds stored template size");
	Check(DeserializeOptions("{\"version\":1,\"futureField\":true}") == disabled,
		"ignores optional fields added by compatible writers");
	Check(MigrateOptions({ u"XTUBKA"_q }, QString()).hideButtonPosts,
		"migrates the previous XTUBKA rule");
	Check(MigrateOptions({ u"other"_q, u"xtubka"_q }, QString()).hideButtonPosts,
		"migration recognizes active aliases without case sensitivity");
	Check(!MigrateOptions({ u"XTUBKA_news"_q }, QString()).hideButtonPosts,
		"migration does not match unrelated channels");
	Check(MigrateOptions({ u"DTEKinfo"_q }, u"12.1"_q).scheduleGroup == u"12.1"_q,
		"preserves the selected outage group");
	Check(MigrateOptions({}, QString()) == disabled,
		"new channels have no filters enabled");
	const auto migrated = MigrateOptions({ u"XTUBKA"_q }, u"2.1"_q);
	Check(DeserializeOptions(SerializeOptions(migrated)) == migrated,
		"migrated rules survive restart");
	std::cout << (TotalChecks - FailedChecks) << "/" << TotalChecks
		<< " checks passed." << std::endl;
	return FailedChecks ? 1 : 0;
}
