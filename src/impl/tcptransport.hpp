/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 * Copyright (c) 2024 ACE migration
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_TCP_TRANSPORT_H
#define RTC_IMPL_TCP_TRANSPORT_H

#include "common.hpp"
#include "queue.hpp"
#include "socket.hpp"
#include "transport.hpp"

#if RTC_ENABLE_WEBSOCKET

#include <ace/net.h>
#include <ace/futures/channel.h>
#include <ace/core/async.h>
#include <ace/core/dispatcher.h>

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <tuple>

namespace rtc::impl {

class TcpTransport final : public Transport, public std::enable_shared_from_this<TcpTransport> {
public:
	using amount_callback = std::function<void(size_t amount)>;

	TcpTransport(string hostname, string service, state_callback callback); // active
	TcpTransport(socket_t sock, state_callback callback);                   // passive
	~TcpTransport();

	void onBufferedAmount(amount_callback callback);
	void setConnectTimeout(std::chrono::milliseconds connectTimeout);
	void setReadTimeout(std::chrono::milliseconds readTimeout);

	void start() override;
	bool send(message_ptr message) override;

	void incoming(message_ptr message) override;
	bool outgoing(message_ptr message) override;

	bool isActive() const;
	string remoteAddress() const;

private:
	// ACE coroutine helpers
	ace::task connectCoroutine();
	ace::task passiveCoroutine();
	ace::task recvLoop();
	ace::task sendLoop();
	void scheduleFlush();

	const bool mIsActive;
	string mHostname, mService;
	amount_callback mBufferedAmountCallback;
	optional<std::chrono::milliseconds> mConnectTimeout;
	optional<std::chrono::milliseconds> mReadTimeout;

	// ACE connection (move-only type, use unique_ptr)
	std::unique_ptr<ace::net::connection> mConn;

	// Channel from sync send() to coroutine sendLoop
	std::shared_ptr<ace::futures::channel<message_ptr>> mSendChannel;
	std::atomic<bool> mSendLoopActive{false};
	std::atomic<bool> mRunning{true};

	// Legacy fields (kept for buffered-amount tracking, mutex for thread-safe send)
	Queue<message_ptr> mSendQueue;
	std::mutex mSendMutex;
	size_t mBufferedAmount = 0;

	// Passive-mode socket (before creating ace::net entity)
	socket_t mPassiveSock = INVALID_SOCKET;
};

} // namespace rtc::impl

#endif

#endif
