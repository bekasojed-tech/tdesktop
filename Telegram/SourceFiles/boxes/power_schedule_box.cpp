#include "boxes/power_schedule_box.h"

#include "data/data_peer.h"
#include "history/view/history_view_power_schedule.h"
#include "history/view/history_view_power_schedule_parse.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"

#include "styles/style_layers.h"

void PowerScheduleBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer) {
	box->setTitle(tr::lng_power_schedule_title());
	const auto current = HistoryView::PowerScheduleGroup(peer);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		peer->name(),
		st::boxLabel));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_power_schedule_about(),
		st::boxLabel));
	const auto enabled = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_power_schedule_enable(),
		!current.isEmpty()));
	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_power_schedule_group(),
		current.isEmpty() ? u"12.1"_q : current));
	input->setMaxLength(6);
	const auto save = [=] {
		const auto group = enabled->checked()
			? input->getLastText().trimmed()
			: QString();
		if (enabled->checked() && !HistoryView::IsPowerScheduleGroup(group)) {
			input->showError();
			return;
		}
		HistoryView::SetPowerScheduleGroup(peer, group);
		box->closeBox();
	};
	input->submits() | rpl::on_next(save, box->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	box->setFocusCallback([=] { input->setFocusFast(); });
}
