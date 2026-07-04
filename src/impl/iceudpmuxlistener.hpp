/**
 * Copyright (c) 2025 Alex Potsides
 * Copyright (c) 2025 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_ICE_UDP_MUX_LISTENER_H
#define RTC_IMPL_ICE_UDP_MUX_LISTENER_H

#include "common.hpp"

#include "rtc/iceudpmuxlistener.hpp"

#include <ace/net.h>
#include <ace/futures/timeout.h>
#include <ace/core/async.h>
#include <ace/core/dispatcher.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace rtc::impl {

struct IceUdpMuxListener final {
	IceUdpMuxListener(uint16_t port, optional<string> bindAddress = nullopt);
	~IceUdpMuxListener();

	void stop();

	const uint16_t port;
	synchronized_callback<IceUdpMuxRequest> unhandledStunRequestCallback;

private:
	void onStunRequest(const char *local_ufrag, const char *remote_ufrag,
	                   const char *address, uint16_t remote_port);

	ace::task muxIoLoop();

	std::unique_ptr<ace::net::net_interface> mNetIface;
	int mSocketFd = -1;
	std::atomic<bool> mStopped;
};

} // namespace rtc::impl

#endif
