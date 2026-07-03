/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_CHANNEL_H
#define RTC_IMPL_CHANNEL_H

#include "common.hpp"
#include "message.hpp"

#include <coroutine>
#include <ace/futures/channel.h>

#include <atomic>
#include <functional>
#include <memory>

namespace rtc::impl {

struct Channel {
	virtual ~Channel() = default;

	// Internal query interface (sync, non-blocking)
	virtual optional<message_variant> receive() = 0;
	virtual optional<message_variant> peek() = 0;
	virtual size_t availableAmount() const = 0;

	// Triggered by transport layer
	virtual void triggerOpen();
	virtual void triggerClosed();
	virtual void triggerError(string error);
	virtual void triggerAvailable(size_t count);
	virtual void triggerBufferedAmount(size_t amount);

	virtual void flushPendingMessages();
	void resetOpenCallback();
	void resetCallbacks();

	// Legacy callbacks (for C API and internal bridge)
	synchronized_stored_callback<> openCallback;
	synchronized_stored_callback<> closedCallback;
	synchronized_stored_callback<string> errorCallback;
	synchronized_stored_callback<> availableCallback;
	synchronized_stored_callback<> bufferedAmountLowCallback;
	synchronized_callback<message_variant> messageCallback;

	// ACE coroutine channels (for public coroutine API)
	std::shared_ptr<ace::futures::channel<optional<message_variant>>> recvChannel =
	    std::make_shared<ace::futures::channel<optional<message_variant>>>();
	std::shared_ptr<ace::futures::channel<optional<string>>> errorChannel =
	    std::make_shared<ace::futures::channel<optional<string>>>();
	std::shared_ptr<ace::futures::channel<int>> stateChannel =
	    std::make_shared<ace::futures::channel<int>>();
	std::shared_ptr<ace::futures::channel<int>> bufferedAmountLowChannel =
	    std::make_shared<ace::futures::channel<int>>();

	std::atomic<size_t> bufferedAmount = 0;
	std::atomic<size_t> bufferedAmountLowThreshold = 0;

protected:
	std::atomic<bool> mOpenTriggered = false;
};

} // namespace rtc::impl

#endif
