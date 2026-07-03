/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 * Copyright (c) 2024 ACE migration
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "channel.hpp"
#include "internals.hpp"

namespace rtc::impl {

void Channel::triggerOpen() {
	mOpenTriggered = true;

	try { openCallback(); } catch (const std::exception &e) {
		PLOG_WARNING << "Uncaught exception in callback: " << e.what();
	}
	stateChannel->push(0);
	flushPendingMessages();
}

void Channel::triggerClosed() {
	try { closedCallback(); } catch (const std::exception &e) {
		PLOG_WARNING << "Uncaught exception in callback: " << e.what();
	}
	stateChannel->push(1);
	recvChannel->push(nullopt);
}

void Channel::triggerError(string error) {
	try { errorCallback(std::move(error)); } catch (const std::exception &e) {
		PLOG_WARNING << "Uncaught exception in callback: " << e.what();
	}
	errorChannel->push(make_optional(std::move(error)));
}

void Channel::triggerAvailable(size_t count) {
	if (count == 1) {
		try { availableCallback(); } catch (const std::exception &e) {
			PLOG_WARNING << "Uncaught exception in callback: " << e.what();
		}
	}
	flushPendingMessages();
}

void Channel::triggerBufferedAmount(size_t amount) {
	size_t previous = bufferedAmount.exchange(amount);
	size_t threshold = bufferedAmountLowThreshold.load();
	if (previous > threshold && amount <= threshold) {
		try { bufferedAmountLowCallback(); } catch (const std::exception &e) {
			PLOG_WARNING << "Uncaught exception in callback: " << e.what();
		}
		bufferedAmountLowChannel->push(0);
	}
}

void Channel::flushPendingMessages() {
	if (!mOpenTriggered)
		return;

	while (messageCallback) {
		auto next = receive();
		if (!next)
			break;

		try { messageCallback(*next); } catch (const std::exception &e) {
			PLOG_WARNING << "Uncaught exception in callback: " << e.what();
		}
		recvChannel->push(make_optional(std::move(*next)));
	}
}

void Channel::resetOpenCallback() {
	mOpenTriggered = false;
	openCallback = nullptr;
}

void Channel::resetCallbacks() {
	mOpenTriggered = false;
	openCallback = nullptr;
	closedCallback = nullptr;
	errorCallback = nullptr;
	availableCallback = nullptr;
	bufferedAmountLowCallback = nullptr;
	messageCallback = nullptr;
}

} // namespace rtc::impl
