/**
 * Copyright (c) 2025 Alex Potsides
 * Copyright (c) 2025 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "iceudpmuxlistener.hpp"
#include "internals.hpp"
#include "stunprotocol.hpp"

#include <ace/net.h>
#include <ace/futures/timeout.h>
#include <ace/core/async.h>
#include <ace/core/dispatcher.h>

#include <cstring>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace rtc::impl::stun;

namespace rtc::impl {

IceUdpMuxListener::IceUdpMuxListener(uint16_t port, optional<string> bindAddress)
    : port(port), mStopped(false) {
	PLOG_VERBOSE << "Creating IceUdpMuxListener on port " << port;

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		throw std::runtime_error("Failed to create UDP socket for mux listener");

	int enable = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = ::htons(port);

	if (bindAddress) {
		struct addrinfo hints = {}, *result = nullptr;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_NUMERICHOST;
		getaddrinfo(bindAddress->c_str(), nullptr, &hints, &result);
		if (result) {
			addr.sin_addr = ((sockaddr_in *)result->ai_addr)->sin_addr;
			freeaddrinfo(result);
		}
	}

	if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		throw std::runtime_error("Failed to bind mux listener UDP socket");
	}

	mNetIface = std::make_unique<ace::net::net_interface>(sock, false);
	mSocketFd = sock;

	PLOG_DEBUG << "ICE UDP mux listener started on port " << port;

	ace::schedule(muxIoLoop());
}

IceUdpMuxListener::~IceUdpMuxListener() {
	PLOG_VERBOSE << "Destroying IceUdpMuxListener";
	stop();
}

void IceUdpMuxListener::stop() {
	if (mStopped.exchange(true))
		return;
	if (mNetIface)
		mNetIface.reset();
}

void IceUdpMuxListener::onStunRequest(const char *local_ufrag, const char *remote_ufrag,
                                       const char *address, uint16_t remote_port) {
	IceUdpMuxRequest request;
	request.localUfrag = local_ufrag;
	request.remoteUfrag = remote_ufrag;
	request.remoteAddress = address;
	request.remotePort = remote_port;
	unhandledStunRequestCallback(std::move(request));
}

ace::task IceUdpMuxListener::muxIoLoop() {
	char buffer[4096];

	while (!mStopped.load()) {
		sockaddr_storage src_addr;
		ace::io::buffer recv_buf;
		recv_buf.set_msg_name(src_addr);
		int nbytes = co_await mNetIface->recv(recv_buf, 0);

		if (nbytes <= 0 || mStopped.load()) continue;

		if (!is_stun_datagram(buffer, (size_t)nbytes)) continue;

		auto msg_hdr = recv_buf.assemble();

		StunMessage msg;
		if (stun_read((uint8_t *)buffer, (size_t)nbytes, msg) < 0) continue;

		if (msg.msg_class == StunClass::REQUEST &&
		    msg.msg_method == StunMethod::BINDING &&
		    msg.has_integrity &&
		    !msg.credentials.username.empty()) {

			std::string username = msg.credentials.username;
			auto sep = username.find(':');
			if (sep != std::string::npos) {
				std::string local_ufrag = username.substr(0, sep);
				std::string remote_ufrag = username.substr(sep + 1);

				char host[64];
				if (getnameinfo((const sockaddr *)&src_addr, msg_hdr->msg_namelen,
				                host, sizeof(host), nullptr, 0, NI_NUMERICHOST) == 0) {
					uint16_t remote_port = 0;
					if (((const sockaddr *)&src_addr)->sa_family == AF_INET)
						remote_port = ::ntohs(((const sockaddr_in *)&src_addr)->sin_port);
					else if (((const sockaddr *)&src_addr)->sa_family == AF_INET6)
						remote_port = ::ntohs(((const sockaddr_in6 *)&src_addr)->sin6_port);

					onStunRequest(local_ufrag.c_str(), remote_ufrag.c_str(), host, remote_port);
				}
			}
		}
	}
	co_return;
}

} // namespace rtc::impl
