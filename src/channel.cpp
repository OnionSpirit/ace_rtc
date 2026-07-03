/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "channel.hpp"
#include "impl/channel.hpp"
#include "impl/internals.hpp"

namespace rtc {

Channel::~Channel() = default;
Channel::Channel(impl_ptr<impl::Channel> impl) : CheshireCat<impl::Channel>(std::move(impl)) {}

size_t Channel::maxMessageSize() const { return 0; }
size_t Channel::bufferedAmount() const { return impl()->bufferedAmount; }
void Channel::setBufferedAmountLowThreshold(size_t amount) { impl()->bufferedAmountLowThreshold = amount; }

// Legacy callback bridge (for C API, goes away when C API is updated)
void Channel::onOpen(std::function<void()> callback)      { impl()->openCallback = std::move(callback); }
void Channel::onClosed(std::function<void()> callback)    { impl()->closedCallback = std::move(callback); }
void Channel::onError(std::function<void(string error)> cb){ impl()->errorCallback = std::move(cb); }
void Channel::onMessage(std::function<void(message_variant data)> cb) {
	impl()->messageCallback = std::move(cb);
	impl()->flushPendingMessages();
}
void Channel::onMessage(std::function<void(binary)> bc, std::function<void(string)> sc) {
	onMessage([bc = std::move(bc), sc = std::move(sc)](variant<binary, string> data) {
		std::visit(overloaded{bc, sc}, std::move(data));
	});
}
void Channel::onBufferedAmountLow(std::function<void()> cb){ impl()->bufferedAmountLowCallback = std::move(cb); }
void Channel::onAvailable(std::function<void()> cb)        { impl()->availableCallback = std::move(cb); }
void Channel::resetCallbacks()                              { impl()->resetCallbacks(); }
optional<message_variant> Channel::receiveSync()            { return impl()->receive(); }
optional<message_variant> Channel::peek()                   { return impl()->peek(); }
size_t Channel::availableAmount() const                     { return impl()->availableAmount(); }

// --- Coroutine API ---
ace::async<optional<message_variant>> Channel::receive() {
	auto data = co_await impl()->recvChannel->pull();
	if (!data) co_return nullopt;
	co_return *data;
}
ace::async<> Channel::onOpen()  { co_await impl()->stateChannel->pull(); co_return; }
ace::async<> Channel::onClose() {
	while (true) { auto s = co_await impl()->stateChannel->pull(); if (s == 1) co_return; }
}
ace::async<string> Channel::onError() {
	auto e = co_await impl()->errorChannel->pull();
	if (e) co_return std::move(*e);
	co_return string{};
}
ace::async<> Channel::onBufferedAmountLow() { co_await impl()->bufferedAmountLowChannel->pull(); co_return; }

} // namespace rtc
