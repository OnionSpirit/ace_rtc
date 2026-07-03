/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 * Copyright (c) 2024 ACE migration
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_CHANNEL_H
#define RTC_CHANNEL_H

#include "common.hpp"

#include <ace/core/async.h>

namespace rtc {

namespace impl {
struct Channel;
}

class RTC_CPP_EXPORT Channel : private CheshireCat<impl::Channel> {
public:
	virtual ~Channel();

	// Sync core (callable from any thread)
	virtual void close() = 0;
	virtual bool send(message_variant data) = 0;
	virtual bool send(const byte *data, size_t size) = 0;

	// Sync queries
	virtual bool isOpen() const = 0;
	virtual bool isClosed() const = 0;
	virtual size_t maxMessageSize() const;
	virtual size_t bufferedAmount() const;

	// --- Coroutine API (ACE-based) ---
	ace::async<optional<message_variant>> receive();
	ace::async<> onOpen();
	ace::async<> onClose();
	ace::async<string> onError();
	ace::async<> onBufferedAmountLow();
	void setBufferedAmountLowThreshold(size_t amount);

	// Internal: used by C API wrappers only
	void onOpen(std::function<void()> callback);
	void onClosed(std::function<void()> callback);
	void onError(std::function<void(string error)> callback);
	void onMessage(std::function<void(message_variant data)> callback);
	void onMessage(std::function<void(binary data)> binaryCallback,
	               std::function<void(string data)> stringCallback);
	void onBufferedAmountLow(std::function<void()> callback);
	void onAvailable(std::function<void()> callback);
	void resetCallbacks();
	optional<message_variant> receiveSync();
	optional<message_variant> peek();
	size_t availableAmount() const;

protected:
	Channel(impl_ptr<impl::Channel> impl);
};

} // namespace rtc

#endif // RTC_CHANNEL_H
