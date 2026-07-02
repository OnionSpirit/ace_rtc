/**
 * Copyright (c) 2021 Staz Modrzynski
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "logcounter.hpp"

#include <ace/core/dispatcher.h>
#include <ace/futures/timeout.h>

namespace rtc::impl {

LogCounter::LogCounter(plog::Severity severity, const std::string &text,
                       std::chrono::seconds duration) {
	mData = std::make_shared<LogData>();
	mData->mDuration = duration;
	mData->mSeverity = severity;
	mData->mText = text;
}

LogCounter &LogCounter::operator++(int) {
	if (mData->mCount++ == 0) {
		ace::schedule([](std::weak_ptr<LogData> data, auto dur) -> ace::task {
			co_await ace::futures::timeout(std::chrono::duration_cast<std::chrono::milliseconds>(dur));
			if (auto ptr = data.lock()) {
				int countCopy;
				countCopy = ptr->mCount.exchange(0);
				PLOG(ptr->mSeverity)
				    << ptr->mText << ": " << countCopy << " (over "
				        << std::chrono::duration_cast<std::chrono::seconds>(ptr->mDuration).count()
				        << " seconds)";
			} else {
				co_return;
			}
		}(mData, mData->mDuration));
	}
	return *this;
}

} // namespace rtc::impl
