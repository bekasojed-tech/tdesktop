#pragma once

class PeerData;

namespace Ui {
class GenericBox;
} // namespace Ui

void PowerScheduleBox(
	not_null<Ui::GenericBox*> box,
	not_null<PeerData*> peer);
