/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "tcptransport.hpp"
#include "internals.hpp"

#if RTC_ENABLE_WEBSOCKET

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include <ace/futures/timeout.h>
#include <chrono>
#include <cstdlib>

namespace rtc::impl {

using namespace std::chrono_literals;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

static uint16_t parsePort(const string &service) {
	return static_cast<uint16_t>(std::stoul(service));
}

TcpTransport::TcpTransport(string hostname, string service, state_callback callback)
    : Transport(nullptr, std::move(callback)), mIsActive(true),
      mHostname(std::move(hostname)), mService(std::move(service)),
      mSendChannel(std::make_shared<ace::futures::channel<message_ptr>>()) {

	PLOG_DEBUG << "Initializing TCP transport (ACE)";
}

TcpTransport::TcpTransport(socket_t sock, state_callback callback)
    : Transport(nullptr, std::move(callback)), mIsActive(false),
      mPassiveSock(sock),
      mSendChannel(std::make_shared<ace::futures::channel<message_ptr>>()) {

	PLOG_DEBUG << "Initializing TCP transport with socket (ACE)";

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if (::getpeername(mPassiveSock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen) == 0) {
		char node[MAX_NUMERICNODE_LEN];
		char serv[MAX_NUMERICSERV_LEN];
		if (::getnameinfo(reinterpret_cast<struct sockaddr *>(&addr), addrlen, node,
		                  MAX_NUMERICNODE_LEN, serv, MAX_NUMERICSERV_LEN,
		                  NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			mHostname = node;
			mService = serv;
		}
	}
}

TcpTransport::~TcpTransport() {
	if (mPassiveSock != INVALID_SOCKET) {
		::closesocket(mPassiveSock);
		mPassiveSock = INVALID_SOCKET;
	}
}

void TcpTransport::onBufferedAmount(amount_callback callback) {
	mBufferedAmountCallback = std::move(callback);
}

void TcpTransport::setConnectTimeout(std::chrono::milliseconds connectTimeout) {
	mConnectTimeout = connectTimeout;
}

void TcpTransport::setReadTimeout(std::chrono::milliseconds readTimeout) {
	mReadTimeout = readTimeout;
}

void TcpTransport::start() {
	if (mIsActive) {
		ace::schedule(connectCoroutine());
	} else {
		ace::schedule(passiveCoroutine());
	}
}

bool TcpTransport::send(message_ptr message) {
	if (state() != State::Connected)
		throw std::runtime_error("Connection is not open");

	if (!message || message->size() == 0)
		return true;

	PLOG_VERBOSE << "Send size=" << message->size();

	{
		std::lock_guard lock(mSendMutex);
		mBufferedAmount += message->size();
	}

	mSendChannel->push(message);
	scheduleFlush();
	return true;
}

void TcpTransport::incoming(message_ptr message) {
	if (!message)
		return;

	PLOG_VERBOSE << "Incoming size=" << message->size();
	recv(message);
}

bool TcpTransport::outgoing(message_ptr message) {
	PLOG_VERBOSE << "Outgoing (ACE path) size=" << message->size();
	mSendChannel->push(message);
	{
		std::lock_guard lock(mSendMutex);
		mBufferedAmount += message->size();
	}
	scheduleFlush();
	return true;
}

bool TcpTransport::isActive() const { return mIsActive; }
string TcpTransport::remoteAddress() const { return mHostname + ':' + mService; }

void TcpTransport::scheduleFlush() {
	bool expected = false;
	if (mSendLoopActive.compare_exchange_strong(expected, true)) {
		ace::schedule(sendLoop());
	}
}

ace::task TcpTransport::connectCoroutine() {
	PLOG_DEBUG << "Connecting to " << mHostname << ":" << mService;
	changeState(State::Connecting);

	try {
		auto mapping = co_await ace::net::socket_tcp();
		if (!mapping) {
			PLOG_WARNING << "TCP socket creation failed";
			changeState(State::Failed);
			co_return;
		}

		auto stream = co_await mapping.bind("0.0.0.0", 0);
		if (!stream) {
			PLOG_WARNING << "TCP bind failed";
			changeState(State::Failed);
			co_return;
		}

		auto port = parsePort(mService);
		auto connVal = co_await stream.connect(mHostname, port);
		if (!connVal) {
			PLOG_WARNING << "TCP connection failed";
			changeState(State::Failed);
			co_return;
		}

		auto [fd, closed] = connVal.extract();
		mConn.reset(new ace::net::connection(fd, closed));
		PLOG_INFO << "TCP connected";
		changeState(State::Connected);

		co_await recvLoop();

	} catch (const std::exception &e) {
		PLOG_WARNING << "TCP connect error: " << e.what();
		changeState(State::Failed);
	}
}

ace::task TcpTransport::passiveCoroutine() {
	try {
		PLOG_DEBUG << "Configuring passive TCP socket";
		changeState(State::Connected);

		socket_t sock = std::exchange(mPassiveSock, INVALID_SOCKET);

		// Construct ace::net::connection from raw fd
		mConn.reset(new ace::net::connection(static_cast<int>(sock), false));

		co_await recvLoop();
	} catch (const std::exception &e) {
		PLOG_ERROR << "Passive TCP error: " << e.what();
		changeState(State::Disconnected);
	}
}

ace::task TcpTransport::recvLoop() {
	try {
		std::string buf;
		while (mConn && *mConn) {
			int len = co_await mConn->recv(buf, 0);
			if (len <= 0) {
				PLOG_DEBUG << "TCP recv closed, len=" << len;
				break;
			}
			incoming(make_message(
			    reinterpret_cast<const byte *>(buf.data()),
			    reinterpret_cast<const byte *>(buf.data()) + len));
		}
	} catch (const std::exception &e) {
		PLOG_ERROR << "TCP recv error: " << e.what();
	}

	PLOG_INFO << "TCP disconnected";
	mRunning = false;
	// Wake sendLoop which is blocked on pull()
	message_ptr wakeMsg;
	mSendChannel->push(wakeMsg);
	changeState(State::Disconnected);
	recv(nullptr);
}

ace::task TcpTransport::sendLoop() {
	try {
		while (mRunning) {
			auto msg = co_await mSendChannel->pull();
			if (!msg || !mRunning)
				break;

			auto data = reinterpret_cast<const char *>(msg->data());
			auto size = static_cast<int>(msg->size());

			while (size > 0 && mRunning) {
				int sent = co_await mConn->send(data, size);
				if (sent < 0) {
					PLOG_ERROR << "TCP send error";
					co_return;
				}
				data += sent;
				size -= sent;
			}

			{
				std::lock_guard lock(mSendMutex);
				mBufferedAmount -= msg->size();
			}
		}
	} catch (const std::exception &e) {
		PLOG_ERROR << "TCP send loop error: " << e.what();
	}

	mSendLoopActive = false;
}

} // namespace rtc::impl

#endif
