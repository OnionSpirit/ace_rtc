/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 * Copyright (c) 2024 ACE migration
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "track.hpp"
#include "internals.hpp"
#include "logcounter.hpp"
#include "peerconnection.hpp"
#include "rtp.hpp"

namespace rtc::impl {

static LogCounter COUNTER_MEDIA_BAD_DIRECTION(plog::warning,
                                              "Number of media packets sent in invalid directions");
static LogCounter COUNTER_QUEUE_FULL(plog::warning,
                                     "Number of media packets dropped due to a full queue");

Track::Track(weak_ptr<PeerConnection> pc, Description::Media desc)
    : mPeerConnection(pc), mMediaDescription(std::move(desc)),
      mRecvQueue(RECV_QUEUE_LIMIT, [](const message_ptr &m) { return m->size(); }) {}

Track::~Track() {
	PLOG_VERBOSE << "Destroying Track";
	try {
		close();
	} catch (const std::exception &e) {
		PLOG_ERROR << e.what();
	}
}

string Track::mid() const {
	std::shared_lock lock(mMutex);
	return mMediaDescription.mid();
}

Description::Direction Track::direction() const {
	std::shared_lock lock(mMutex);
	return mMediaDescription.direction();
}

Description::Media Track::description() const {
	std::shared_lock lock(mMutex);
	return mMediaDescription;
}

void Track::setDescription(Description::Media desc) {
	{
		std::unique_lock lock(mMutex);
		if (desc.mid() != mMediaDescription.mid())
			throw std::logic_error("Media description mid does not match track mid");

		mMediaDescription = std::move(desc);
	}

	if (auto handler = getMediaHandler())
		handler->mediaChain(description());
}

void Track::close() {
	PLOG_VERBOSE << "Closing Track";

	if (!mIsClosed.exchange(true)) {
		triggerClosed();
		setMediaHandler(nullptr);
	}
}

message_variant Track::trackMessageToVariant(message_ptr message) {
	if (message->type == Message::Control)
		return to_variant(*message);
	else
		return to_variant(std::move(*message));
}

optional<message_variant> Track::receive() {
	if (auto next = mRecvQueue.pop()) {
		return trackMessageToVariant(*next);
	}
	return nullopt;
}

optional<message_variant> Track::peek() {
	if (auto next = mRecvQueue.peek()) {
		return trackMessageToVariant(*next);
	}
	return nullopt;
}

size_t Track::availableAmount() const { return mRecvQueue.amount(); }

bool Track::isOpen(void) const {
#if RTC_ENABLE_MEDIA
	std::shared_lock lock(mMutex);
	return !mIsClosed && mDtlsSrtpTransport.lock();
#else
	return false;
#endif
}

bool Track::isClosed(void) const { return mIsClosed; }

size_t Track::maxMessageSize() const {
	optional<size_t> mtu;
	if (auto pc = mPeerConnection.lock())
		mtu = pc->config.mtu;

	return mtu.value_or(DEFAULT_MTU) - 12 - 8 - 40; // SRTP/UDP/IPv6
}

#if RTC_ENABLE_MEDIA
void Track::open(shared_ptr<DtlsSrtpTransport> transport) {
	{
		std::lock_guard lock(mMutex);
		mDtlsSrtpTransport = transport;
	}

	if (!mIsClosed)
		triggerOpen();
}
#endif

void Track::incoming(message_ptr message) {
	if (!message)
		return;

	auto dir = direction();
	if ((dir == Description::Direction::SendOnly || dir == Description::Direction::Inactive) &&
	    message->type != Message::Control) {
		COUNTER_MEDIA_BAD_DIRECTION++;
		return;
	}

	message_vector messages{std::move(message)};
	if (auto handler = getMediaHandler()) {
		try {
			handler->incomingChain(messages, [weak_this = weak_from_this()](message_ptr m) {
				if (auto locked = weak_this.lock()) {
					locked->transportSend(m);
				}
			});
		} catch (const std::exception &e) {
			PLOG_WARNING << "Exception in incoming media handler: " << e.what();
			return;
		}
	}

	for (auto &m : messages) {
		if (mRecvQueue.full()) {
			COUNTER_QUEUE_FULL++;
			return;
		}

		mRecvQueue.push(m);
		triggerAvailable(mRecvQueue.size());
	}
}

bool Track::outgoing(message_ptr message) {
	if (mIsClosed)
		throw std::runtime_error("Track is closed");

	auto handler = getMediaHandler();

	if (!handler && IsRtcp(*message))
		message->type = Message::Control;

	auto dir = direction();
	if ((dir == Description::Direction::RecvOnly || dir == Description::Direction::Inactive) &&
	    message->type != Message::Control) {
		COUNTER_MEDIA_BAD_DIRECTION++;
		return false;
	}

	if (handler) {
		message_vector messages{std::move(message)};
		handler->outgoingChain(messages, [weak_this = weak_from_this()](message_ptr m) {
			if (auto locked = weak_this.lock()) {
				locked->transportSend(m);
			}
		});

		bool ret = false;
		for (auto &m : messages)
			ret = transportSend(std::move(m));

		return ret;

	} else {
		return transportSend(std::move(message));
	}
}

bool Track::transportSend([[maybe_unused]] message_ptr message) {
#if RTC_ENABLE_MEDIA
	shared_ptr<DtlsSrtpTransport> transport;
	{
		std::shared_lock lock(mMutex);
		transport = mDtlsSrtpTransport.lock();
		if (!transport)
			throw std::runtime_error("Track is not open");

		if (mMediaDescription.type() == "audio")
			message->dscp = 46; // EF
		else
			message->dscp = 36; // AF42
	}

	return transport->sendMedia(message);
#else
	throw std::runtime_error("Track is disabled (not compiled with media support)");
#endif
}

void Track::setMediaHandler(shared_ptr<MediaHandler> handler) {
	{
		std::unique_lock lock(mMutex);
		mMediaHandler = handler;
	}

	if (handler)
		handler->mediaChain(description());
}

shared_ptr<MediaHandler> Track::getMediaHandler() {
	std::shared_lock lock(mMutex);
	return mMediaHandler;
}

void Track::flushPendingMessages() {
	if (!mOpenTriggered)
		return;

	while (true) {
		auto next = mRecvQueue.pop();
		if (!next)
			break;

		auto message = next.value();
		auto variant = trackMessageToVariant(message);
		recvChannel->push(make_optional(std::move(variant)));
	}
}

} // namespace rtc::impl
