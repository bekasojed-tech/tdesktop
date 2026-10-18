#include "boxes/channel_filters_box.h"

#include "data/data_peer.h"
#include "history/view/history_view_power_schedule_parse.h"
#include "history/history_item_filter.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"

#include "styles/style_layers.h"

void ChannelFiltersBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer) {
	box->setTitle(tr::lng_channel_filters_title());
	const auto current = HistoryFilter::Options(peer);
	box->addRow(object_ptr<Ui::FlatLabel>(box, peer->name(), st::boxLabel));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_channel_filters_about(),
		st::boxLabel));
	const auto hideButtons = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_channel_filters_hide_buttons(),
		current.hideButtonPosts));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_channel_filters_hide_buttons_about(),
		st::boxLabel));
	const auto schedules = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_power_schedule_enable(),
		!current.scheduleGroup.isEmpty()));
	const auto group = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_power_schedule_group(),
		current.scheduleGroup.isEmpty() ? u"12.1"_q : current.scheduleGroup));
	group->setMaxLength(6);
	const auto format = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_power_schedule_format(),
		current.scheduleFormat.isEmpty()
			? HistoryView::DefaultPowerScheduleFormat()
			: current.scheduleFormat));
	format->setMaxLength(500);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_power_schedule_format_about(),
		st::boxLabel));
	const auto updateFields = [=] {
		group->setDisabled(!schedules->checked());
		format->setDisabled(!schedules->checked());
	};
	updateFields();
	schedules->checkedChanges() | rpl::on_next(updateFields, box->lifetime());
	const auto save = [=] {
		const auto selectedGroup = group->getLastText().trimmed();
		const auto selectedFormat = format->getLastText().trimmed();
		if (schedules->checked()) {
			if (!HistoryView::IsPowerScheduleGroup(selectedGroup)) {
				group->showError();
				return;
			} else if (!HistoryView::IsPowerScheduleFormat(selectedFormat)) {
				format->showError();
				return;
			}
		}
		HistoryFilter::SetOptions(peer, {
			.hideButtonPosts = hideButtons->checked(),
			.scheduleGroup = schedules->checked() ? selectedGroup : QString(),
			.scheduleFormat = schedules->checked()
				? selectedFormat
				: current.scheduleFormat,
		});
		box->closeBox();
	};
	group->submits() | rpl::on_next(save, box->lifetime());
	format->submits() | rpl::on_next(save, box->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}
