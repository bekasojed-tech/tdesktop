/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Fork {

// Fork-only policy; see docs/fork-ads.md before merging upstream changes.
#if defined TDESKTOP_FORK_DISABLE_ADS && TDESKTOP_FORK_DISABLE_ADS
inline constexpr auto kDisableAds = true;
#else
inline constexpr auto kDisableAds = false;
#endif // TDESKTOP_FORK_DISABLE_ADS

} // namespace Fork
