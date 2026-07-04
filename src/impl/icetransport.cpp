/**
 * Copyright (c) 2019-2020 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "icetransport.hpp"
#include "configuration.hpp"
#include "internals.hpp"
#include "utils.hpp"

#include <ace/net.h>
#include <ace/futures/timeout.h>
#include <ace/futures/channel.h>
#include <ace/core/async.h>
#include <ace/core/dispatcher.h>

#include <algorithm>
#include <cstring>
#include <random>

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;
using namespace rtc::impl::ice;
using namespace rtc::impl::stun;
using namespace rtc::impl::turn;

namespace rtc::impl {

// ===================================================================
// Helpers
// ===================================================================

static int64_t now_ms() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	           std::chrono::system_clock::now().time_since_epoch())
	    .count();
}

static int resolve_addr(const std::string &host, uint16_t port, AddrRecord &out) {
	struct addrinfo hints = {}, *result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_ADDRCONFIG;
	std::string ps = std::to_string(port);
	if (getaddrinfo(host.c_str(), ps.c_str(), &hints, &result) != 0) return -1;
	for (auto *ai = result; ai; ai = ai->ai_next) {
		if (ai->ai_family == AF_INET || ai->ai_family == AF_INET6) {
			memcpy(out.addr.data(), ai->ai_addr, ai->ai_addrlen);
			out.len = ai->ai_addrlen;
			out.socktype = ai->ai_socktype;
			freeaddrinfo(result);
			return 0;
		}
	}
	freeaddrinfo(result);
	return -1;
}

// ===================================================================
// Init / Cleanup
// ===================================================================

void IceTransport::Init() {}
void IceTransport::Cleanup() {}

// ===================================================================
// Constructor
// ===================================================================

IceTransport::IceTransport(const Configuration &config, candidate_callback candidateCallback,
                           state_callback stateChangeCallback,
                           gathering_state_callback gatheringStateChangeCallback)
    : Transport(nullptr, std::move(stateChangeCallback)),
      mRole(Description::Role::ActPass),
      mGatheringState(GatheringState::New),
      mCandidateCallback(std::move(candidateCallback)),
      mGatheringStateChangeCallback(std::move(gatheringStateChangeCallback)),
      mConfig(config) {

	PLOG_DEBUG << "Initializing ICE transport (ace::net)";

	// Create UDP socket
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) throw std::runtime_error("Failed to create UDP socket");

	int enable = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = INADDR_ANY;

	if (config.bindAddress) {
		struct addrinfo *result = nullptr;
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_NUMERICHOST;
		getaddrinfo(config.bindAddress->c_str(), nullptr, &hints, &result);
		if (result) {
			bind_addr.sin_addr = ((sockaddr_in *)result->ai_addr)->sin_addr;
			freeaddrinfo(result);
		}
	}

	if (config.portRangeBegin > 1024) {
		bool bound = false;
		for (uint16_t p = config.portRangeBegin;
		     p <= (config.portRangeEnd ? config.portRangeEnd : config.portRangeBegin); ++p) {
			bind_addr.sin_port = htons(p);
			if (bind(sock, (sockaddr *)&bind_addr, sizeof(bind_addr)) == 0) {
				bound = true;
				break;
			}
		}
		if (!bound) {
			close(sock);
			throw std::runtime_error("Failed to bind UDP socket in port range");
		}
	} else {
		bind_addr.sin_port = 0;
		if (bind(sock, (sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
			close(sock);
			throw std::runtime_error("Failed to bind UDP socket");
		}
	}

	int dscp = 0;
	setsockopt(sock, IPPROTO_IP, IP_TOS, &dscp, sizeof(dscp));

	mNetIface = std::make_unique<ace::net::net_interface>(sock, false);
	mSocketFd = sock;

	sockaddr_in actual_addr = {};
	socklen_t addrlen = sizeof(actual_addr);
	getsockname(sock, (sockaddr *)&actual_addr, &addrlen);
	PLOG_INFO << "ICE transport bound to UDP port " << ntohs(actual_addr.sin_port);

	mLocalDescription = create_local_description();
	mTiebreaker = (uint64_t)now_ms() << 16 | ntohs(actual_addr.sin_port);

	ace::schedule(ioLoop());
}

IceTransport::~IceTransport() {
	PLOG_DEBUG << "Destroying ICE transport";
	mRunning.store(false);
	if (mNetIface) mNetIface.reset();
}

// ===================================================================
// Public API
// ===================================================================

Description::Role IceTransport::role() const { return mRole; }

auto IceTransport::gatheringState() const -> GatheringState { return mGatheringState.load(); }

void IceTransport::setIceAttributes(string uFrag, string pwd) {
	if (!::rtc::impl::ice::ice_is_valid_string(uFrag) || !::rtc::impl::ice::ice_is_valid_string(pwd))
		throw std::invalid_argument("Invalid ICE attributes");
	mLocalDescription.ice_ufrag = std::move(uFrag);
	mLocalDescription.ice_pwd = std::move(pwd);
}

Description IceTransport::getLocalDescription(Description::Type type) const {
	std::string sdp = generate_sdp(mLocalDescription);
	Description desc(sdp, type,
	                 type == Description::Type::Offer ? Description::Role::ActPass : mRole);
	desc.addIceOption("trickle");
	return desc;
}

void IceTransport::setRemoteDescription(const Description &description) {
	if (description.type() == Description::Type::Answer &&
	    description.role() == Description::Role::ActPass)
		throw std::invalid_argument("Illegal role actpass in remote answer description");

	if (mRole == Description::Role::ActPass)
		mRole = description.role() == Description::Role::Active
		            ? Description::Role::Passive
		            : Description::Role::Active;

	if (mRole == description.role())
		throw std::invalid_argument("Incompatible roles with remote description");

	mMid = description.bundleMid();
	mIsControlling = (mRole == Description::Role::Active);

	parse_sdp(description.generateApplicationSdp("\r\n"), mRemoteDescription);
	mRemoteCandidates.clear();
	for (auto &c : mRemoteDescription.candidates)
		mRemoteCandidates.push_back(c);

	PLOG_INFO << "Remote description: ufrag=" << mRemoteDescription.ice_ufrag
	          << " candidates=" << mRemoteCandidates.size();

	scheduleChecks();
}

bool IceTransport::addRemoteCandidate(const Candidate &candidate) {
	if (!candidate.isResolved()) return false;
	std::string sdp = static_cast<string>(candidate);
	IceCandidate c;
	if (parse_candidate_sdp(sdp, c) != 0) return false;
	mRemoteCandidates.push_back(c);
	scheduleChecks();
	return true;
}

void IceTransport::gatherLocalCandidates(string mid, std::vector<IceServer> additionalIceServers) {
	mMid = std::move(mid);
	changeGatheringState(GatheringState::InProgress);

	gatherHostCandidates();

	auto servers = mConfig.iceServers;
	for (auto &s : additionalIceServers) servers.push_back(s);

	for (auto &server : servers) {
		if (server.type == IceServer::Type::Stun && !server.hostname.empty()) {
			uint16_t port = server.port ? server.port : 3478;
			gatherSrflxCandidates(server.hostname, port);
			break;
		}
	}

	for (auto &server : servers) {
		if (server.type == IceServer::Type::Turn && !server.hostname.empty()) {
			addIceServer(server);
		}
	}

	PLOG_INFO << "Local candidates: " << mLocalCandidates.size();
	if (mGatheringState.load() == GatheringState::InProgress)
		changeGatheringState(GatheringState::Complete);
}

optional<string> IceTransport::getLocalAddress() const {
	if (mSelectedPair && mSelectedPair->local)
		return mSelectedPair->local->hostname + ":" + mSelectedPair->local->service;
	return nullopt;
}

optional<string> IceTransport::getRemoteAddress() const {
	if (mSelectedPair && mSelectedPair->remote)
		return mSelectedPair->remote->hostname + ":" + mSelectedPair->remote->service;
	return nullopt;
}

bool IceTransport::getSelectedCandidatePair(Candidate *local, Candidate *remote) {
	if (!mSelectedPair) return false;
	if (local) {
		auto sdp = generate_candidate_sdp(*mSelectedPair->local);
		*local = Candidate(sdp, mMid);
		local->resolve(Candidate::ResolveMode::Simple);
	}
	if (remote) {
		auto sdp = generate_candidate_sdp(*mSelectedPair->remote);
		*remote = Candidate(sdp, mMid);
		remote->resolve(Candidate::ResolveMode::Simple);
	}
	return true;
}

bool IceTransport::send(message_ptr message) {
	auto s = state();
	if (!message || (s != State::Connected && s != State::Completed)) return false;
	PLOG_VERBOSE << "Send size=" << message->size();
	return outgoing(message);
}

bool IceTransport::outgoing(message_ptr message) {
	std::lock_guard<std::mutex> lock(mSendMutex);
	if (mSelectedPair && mSelectedPair->remote) {
		return ::sendto(mSocketFd, message->data(), message->size(), 0,
		                (const sockaddr *)mSelectedPair->remote->resolved.addr.data(),
		                mSelectedPair->remote->resolved.len) >= 0;
	}
	// Try TURN channel
	for (auto &ts : mTurnServers) {
		if (!ts.allocated || !ts.data_channel || !ts.resolved_addr.isValid()) continue;
		uint8_t chbuf[4096];
		int n = wrap_channel_data(chbuf, sizeof(chbuf),
		                           (const uint8_t *)message->data(), message->size(),
		                           ts.data_channel);
		if (n > 0) {
			return ::sendto(mSocketFd, chbuf, n, 0,
			                (const sockaddr *)ts.resolved_addr.addr.data(),
			                ts.resolved_addr.len) >= 0;
		}
	}
	return false;
}

// ===================================================================
// Candidate gathering
// ===================================================================

void IceTransport::gatherHostCandidates() {
	AddrRecord rec;
	rec.len = sizeof(sockaddr_in);
	rec.socktype = SOCK_DGRAM;
	auto *sin = (sockaddr_in *)rec.addr.data();

	sockaddr_in bound = {};
	socklen_t addrlen = sizeof(bound);
	getsockname(mSocketFd, (sockaddr *)&bound, &addrlen);
	sin->sin_family = AF_INET;
	sin->sin_port = bound.sin_port;
	sin->sin_addr = bound.sin_addr;

	int idx = (int)mLocalCandidates.size();
	auto cand = create_local_candidate(IceCandidateType::HOST, 1, idx, rec,
	                                    IceCandidateTransport::UDP);
	mLocalCandidates.push_back(cand);
	mLocalDescription.candidates.push_back(cand);

	auto sdp = generate_candidate_sdp(cand);
	mCandidateCallback(Candidate(sdp, mMid));
}

void IceTransport::gatherSrflxCandidates(const string &stunHost, uint16_t stunPort) {
	PLOG_INFO << "STUN binding for srflx: " << stunHost << ":" << stunPort;

	AddrRecord stun_addr;
	if (resolve_addr(stunHost, stunPort, stun_addr) != 0) {
		PLOG_WARNING << "Failed to resolve STUN server: " << stunHost;
		return;
	}

	StunMessage msg;
	msg.msg_class = StunClass::REQUEST;
	msg.msg_method = StunMethod::BINDING;
	random_bytes(msg.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);

	sendStunMessage(stun_addr, msg);

	IceCheck check;
	memcpy(check.transaction_id.data(), msg.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
	check.is_stun_binding = true;
	check.next_retransmit = now_ms() + 500;
	mPendingChecks.push_back(check);
}

void IceTransport::gatherRelayCandidates(const IceServer &server) {
	PLOG_INFO << "TURN allocate: " << server.hostname << ":" << server.port;

	uint16_t port = server.port ? server.port : 3478;

	TurnCtx ts;
	ts.hostname = server.hostname;
	ts.port = port;
	ts.username = server.username;
	ts.password = server.password;

	if (resolve_addr(server.hostname, port, ts.resolved_addr) != 0) {
		PLOG_WARNING << "Failed to resolve TURN server: " << server.hostname;
		return;
	}

	StunMessage msg;
	msg.msg_class = StunClass::REQUEST;
	msg.msg_method = StunMethod::ALLOCATE;
	random_bytes(msg.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
	msg.requested_transport = true;
	msg.lifetime = 600;
	msg.lifetime_set = true;
	if (!ts.username.empty()) msg.credentials.username = ts.username;

	sendStunMessage(ts.resolved_addr, msg, ts.password.empty() ? nullptr : ts.password.c_str());
	mTurnServers.push_back(std::move(ts));
}

void IceTransport::addIceServer(const IceServer &server) {
	gatherRelayCandidates(server);
}

// ===================================================================
// Scheduling
// ===================================================================

void IceTransport::scheduleChecks() {
	if (mLocalCandidates.empty() || mRemoteCandidates.empty()) return;
	if (mGatheringState.load() == GatheringState::New)
		changeGatheringState(GatheringState::InProgress);

	mPairs.clear();
	mPendingChecks.clear();

	for (auto &local : mLocalCandidates) {
		for (auto &remote : mRemoteCandidates) {
			if (local.resolved.isValid() && remote.resolved.isValid()) {
				auto *lsa = (const sockaddr *)local.resolved.addr.data();
				auto *rsa = (const sockaddr *)remote.resolved.addr.data();
				if (lsa->sa_family != rsa->sa_family) continue;
			}
			auto pair = create_pair(&local, &remote, mIsControlling);
			pair.state = IcePairState::PENDING;
			mPairs.push_back(std::move(pair));
		}
	}

	std::sort(mPairs.begin(), mPairs.end(),
	          [](const IceCandidatePair &a, const IceCandidatePair &b) {
		          return a.priority > b.priority;
	          });

	for (auto &pair : mPairs) {
		if (mPendingChecks.size() >= 5) break;
		IceCheck check;
		check.pair = &pair;
		random_bytes(check.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
		check.next_retransmit = now_ms();
		mPendingChecks.push_back(check);
	}

	PLOG_INFO << "Scheduled checks: " << mPendingChecks.size()
	          << " for " << mPairs.size() << " pairs";
}

// ===================================================================
// STUN / send helpers
// ===================================================================

bool IceTransport::sendStunMessage(const AddrRecord &dst, const StunMessage &msg,
                                    const char *password) {
	uint8_t buf[4096];
	memset(buf, 0, sizeof(buf));
	int len = stun_write(buf, sizeof(buf), msg, password);
	if (len <= 0) return false;
	return sendRaw(dst, buf, len) > 0;
}

int IceTransport::sendRaw(const AddrRecord &dst, const void *data, size_t size) {
	if (!mNetIface) return -1;
	std::lock_guard<std::mutex> lock(mSendMutex);
	return (int)::sendto(mSocketFd, data, size, 0,
	                     (const sockaddr *)dst.addr.data(), dst.len);
}

// ===================================================================
// State helpers
// ===================================================================

void IceTransport::changeGatheringState(GatheringState state) {
	if (mGatheringState.exchange(state) != state) {
		try { mGatheringStateChangeCallback(mGatheringState); }
		catch (const std::exception &e) { PLOG_WARNING << e.what(); }
	}
}

void IceTransport::processCandidate(const string &candidate) {
	try { mCandidateCallback(Candidate(candidate, mMid)); }
	catch (const std::exception &e) { PLOG_WARNING << e.what(); }
}

void IceTransport::processGatheringDone() {
	changeGatheringState(GatheringState::Complete);
}

// ===================================================================
// ACE I/O loop
// ===================================================================

ace::task IceTransport::ioLoop() {
	uint8_t buffer[4096];

	while (mRunning.load()) {
		int64_t now = now_ms();

		// Process pending STUN checks
		for (auto &check : mPendingChecks) {
			if (now < check.next_retransmit) continue;
			if (check.retransmits > 7) {
				if (check.pair) check.pair->state = IcePairState::FAILED;
				check.next_retransmit = INT64_MAX;
				continue;
			}

			StunMessage msg;
			msg.msg_class = StunClass::REQUEST;
			msg.msg_method = StunMethod::BINDING;
			memcpy(msg.transaction_id.data(), check.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);

			if (check.is_stun_binding) {
				if (!mTurnServers.empty() && mTurnServers[0].resolved_addr.isValid()) {
					sendStunMessage(mTurnServers[0].resolved_addr, msg);
				}
			} else if (check.pair && check.pair->local && check.pair->remote) {
				auto &local = *check.pair->local;
				auto &remote = *check.pair->remote;

				msg.credentials.username =
				    mRemoteDescription.ice_ufrag + ":" + mLocalDescription.ice_ufrag;
				msg.priority = compute_priority(IceCandidateType::PEER_REFLEXIVE, AF_INET,
				                                local.component, 0,
				                                IceCandidateTransport::UDP);

				if (mIsControlling) {
					msg.ice_controlling = mTiebreaker;
					if (check.pair->nomination_requested) msg.use_candidate = true;
				} else {
					msg.ice_controlled = mTiebreaker;
				}

				sendStunMessage(remote.resolved, msg, mRemoteDescription.ice_pwd.c_str());
			}

			check.retransmits++;
			check.next_retransmit = now_ms() + (500 * (1 << std::min(check.retransmits, 6)));
		}

		mPendingChecks.erase(std::remove_if(mPendingChecks.begin(), mPendingChecks.end(),
		                                     [](const IceCheck &c) {
			                                     return c.next_retransmit == INT64_MAX;
		                                     }),
		                      mPendingChecks.end());

		// TURN refreshes
		for (auto &ts : mTurnServers) {
			if (!ts.allocated || !ts.resolved_addr.isValid()) continue;
			if (now_ms() > ts.allocation_expiry - 60000) {
				StunMessage refresh;
				refresh.msg_class = StunClass::REQUEST;
				refresh.msg_method = StunMethod::REFRESH;
				random_bytes(refresh.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
				refresh.lifetime = 600;
				refresh.lifetime_set = true;
				if (!ts.username.empty()) refresh.credentials.username = ts.username;
				sendStunMessage(ts.resolved_addr, refresh,
				                ts.password.empty() ? nullptr : ts.password.c_str());
				ts.allocation_expiry = now_ms() + 600000;
			}
		}

		// Receive
		sockaddr_storage src_addr_storage;
		ace::io::buffer recv_buf;
		recv_buf.set_msg_name(src_addr_storage);
		auto n = co_await mNetIface->recv(recv_buf);
		if (not n or not mRunning.load()) continue;

		auto hdr = recv_buf.assemble();

		AddrRecord src;
		memcpy(src.addr.data(), &src_addr_storage, hdr->msg_namelen);
		src.len = hdr->msg_namelen;
		src.socktype = SOCK_DGRAM;

		if (is_stun_datagram(buffer, n)) {
			StunMessage recv_msg;
			if (stun_read(buffer, n, recv_msg) < 0) continue;

			if (recv_msg.msg_class == StunClass::RESP_SUCCESS &&
			    recv_msg.msg_method == StunMethod::BINDING) {

				// STUN server binding response — create srflx
				if (recv_msg.mapped.isValid()) {
					for (auto &check : mPendingChecks) {
						if (!check.is_stun_binding) continue;
						if (memcmp(check.transaction_id.data(), recv_msg.transaction_id.data(),
						           STUN_TRANSACTION_ID_SIZE) == 0) {
							int idx = (int)mLocalCandidates.size();
							auto cand = create_local_candidate(IceCandidateType::SERVER_REFLEXIVE,
							                                   1, idx, recv_msg.mapped,
							                                   IceCandidateTransport::UDP);
							cand.foundation = "srflx-" + std::to_string(idx);
							mLocalCandidates.push_back(cand);
							mLocalDescription.candidates.push_back(cand);

							auto sdp = generate_candidate_sdp(cand);
							PLOG_INFO << "Discovered srflx candidate: " << sdp;
							mCandidateCallback(Candidate(sdp, mMid));

							check.next_retransmit = INT64_MAX;
							scheduleChecks();
							break;
						}
					}
				}

				// Connectivity check response
				for (auto &check : mPendingChecks) {
					if (!check.pair || check.is_stun_binding) continue;
					if (memcmp(check.transaction_id.data(), recv_msg.transaction_id.data(),
					           STUN_TRANSACTION_ID_SIZE) == 0) {
						check.pair->state = IcePairState::SUCCEEDED;
						check.next_retransmit = INT64_MAX;
						mSelectedPair = check.pair;
						if (state() == State::Connecting)
							changeState(State::Connected);
						break;
					}
				}
			}
			else if (recv_msg.msg_class == StunClass::REQUEST &&
			         recv_msg.msg_method == StunMethod::BINDING &&
			         recv_msg.has_integrity) {
				// Incoming connectivity check
				if (!stun_check_integrity(buffer, n, recv_msg,
				                           mLocalDescription.ice_pwd.c_str())) continue;

				StunMessage resp;
				resp.msg_class = StunClass::RESP_SUCCESS;
				resp.msg_method = StunMethod::BINDING;
				memcpy(resp.transaction_id.data(), recv_msg.transaction_id.data(),
				       STUN_TRANSACTION_ID_SIZE);
				resp.mapped = src;
				sendStunMessage(src, resp, mLocalDescription.ice_pwd.c_str());

				if (state() == State::Disconnected)
					changeState(State::Connecting);

				if (recv_msg.use_candidate && state() == State::Connecting)
					changeState(State::Connected);
			}
			else if (recv_msg.msg_method == StunMethod::ALLOCATE ||
			         recv_msg.msg_method == StunMethod::CREATE_PERMISSION ||
			         recv_msg.msg_method == StunMethod::CHANNEL_BIND) {
				// TURN response
				for (auto &ts : mTurnServers) {
					if (!ts.resolved_addr.isValid()) continue;
					if (recv_msg.msg_class == StunClass::RESP_SUCCESS) {
						if (recv_msg.msg_method == StunMethod::ALLOCATE) {
							ts.relayed_addr = recv_msg.relayed;
							ts.allocated = true;
							ts.allocation_expiry = now_ms() + recv_msg.lifetime * 1000;

							int idx = (int)mLocalCandidates.size();
							auto cand = create_local_candidate(IceCandidateType::RELAYED,
							                                   1, idx, recv_msg.relayed,
							                                   IceCandidateTransport::UDP);
							cand.foundation = "relay-" + std::to_string(idx);
							mLocalCandidates.push_back(cand);
							mLocalDescription.candidates.push_back(cand);

							auto sdp = generate_candidate_sdp(cand);
							PLOG_INFO << "Discovered relay candidate: " << sdp;
							mCandidateCallback(Candidate(sdp, mMid));
							scheduleChecks();

							if (!mRemoteCandidates.empty()) {
								StunMessage perm;
								perm.msg_class = StunClass::REQUEST;
								perm.msg_method = StunMethod::CREATE_PERMISSION;
								random_bytes(perm.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
								perm.peers.push_back(mRemoteCandidates.front().resolved);
								if (!ts.username.empty())
									perm.credentials.username = ts.username;
								sendStunMessage(ts.resolved_addr, perm,
								                ts.password.empty() ? nullptr : ts.password.c_str());
							}
						}
					}
					break;
				}
			}
		}
		else if (is_channel_data(buffer, n)) {
			auto *ch = (const ChannelDataHeader *)buffer;
			uint16_t data_len = ::ntohs(ch->length);
			const auto *app_data = buffer + CHANNEL_DATA_HEADER_SIZE;
			if (data_len <= (size_t)n - CHANNEL_DATA_HEADER_SIZE) {
				PLOG_VERBOSE << "Incoming channel data size=" << data_len;
				incoming(make_message((const byte *)app_data, (const byte *)app_data + data_len));
			}
		}
		else {
			PLOG_VERBOSE << "Incoming app data size=" << n;
			incoming(make_message((const byte *)buffer, (const byte *)buffer + n));
			if (state() == State::Disconnected) changeState(State::Connecting);
		}

		// State transitions
		bool any_succeeded = false;
		for (auto &p : mPairs)
			if (p.state == IcePairState::SUCCEEDED) { any_succeeded = true; break; }

		if (any_succeeded && state() != State::Connected && state() != State::Completed)
			changeState(State::Connected);

		if (mIsControlling && mSelectedPair && !mSelectedPair->nomination_requested &&
		    state() == State::Connected) {
			mSelectedPair->nomination_requested = true;
			IceCheck nc;
			nc.pair = mSelectedPair;
			random_bytes(nc.transaction_id.data(), STUN_TRANSACTION_ID_SIZE);
			nc.next_retransmit = now_ms();
			mPendingChecks.push_back(nc);
		}
	}

	co_return;
}

} // namespace rtc::impl
