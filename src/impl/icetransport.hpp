/**
 * Copyright (c) 2019-2020 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_ICE_TRANSPORT_H
#define RTC_IMPL_ICE_TRANSPORT_H

#include "candidate.hpp"
#include "common.hpp"
#include "configuration.hpp"
#include "description.hpp"
#include "global.hpp"
#include "iceprotocol.hpp"
#include "peerconnection.hpp"
#include "stunprotocol.hpp"
#include "transport.hpp"
#include "turnprotocol.hpp"

#include <ace/net.h>
#include <ace/futures/timeout.h>
#include <ace/futures/channel.h>
#include <ace/core/async.h>
#include <ace/core/dispatcher.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace rtc::impl {

class IceTransport : public Transport {
public:
	static void Init();
	static void Cleanup();

	enum class GatheringState { New = 0, InProgress = 1, Complete = 2 };

	using candidate_callback = std::function<void(const Candidate &candidate)>;
	using gathering_state_callback = std::function<void(GatheringState state)>;

	IceTransport(const Configuration &config, candidate_callback candidateCallback,
	             state_callback stateChangeCallback,
	             gathering_state_callback gatheringStateChangeCallback);
	~IceTransport();

	Description::Role role() const;
	GatheringState gatheringState() const;
	Description getLocalDescription(Description::Type type) const;
	void setRemoteDescription(const Description &description);
	bool addRemoteCandidate(const Candidate &candidate);
	void gatherLocalCandidates(string mid, std::vector<IceServer> additionalIceServers = {});
	void setIceAttributes(string uFrag, string pwd);

	optional<string> getLocalAddress() const;
	optional<string> getRemoteAddress() const;

	bool send(message_ptr message) override;

	bool getSelectedCandidatePair(Candidate *local, Candidate *remote);

private:
	bool outgoing(message_ptr message) override;

	void changeGatheringState(GatheringState state);
	void processCandidate(const string &candidate);
	void processGatheringDone();

	void addIceServer(const IceServer &server);

	// ICE protocol
	void initIceParameters(string uFrag, string pwd);
	void gatherHostCandidates();
	void gatherSrflxCandidates(const string &stunHost, uint16_t stunPort);
	void gatherRelayCandidates(const IceServer &server);
	void scheduleChecks();

	bool sendStunMessage(const ice::AddrRecord &dst, const stun::StunMessage &msg,
	                     const char *password = nullptr);
	int sendRaw(const ice::AddrRecord &dst, const void *data, size_t size);

	// I/O loop
	ace::task ioLoop();

	// Internal types
	struct IceCheck {
		ice::IceCandidatePair *pair = nullptr;
		std::array<uint8_t, stun::STUN_TRANSACTION_ID_SIZE> transaction_id{};
		int retransmits = 0;
		int64_t next_retransmit = 0;
		bool is_stun_binding = false;
	};

	struct TurnCtx {
		std::string hostname;
		uint16_t port = 0;
		std::string username;
		std::string password;
		ice::AddrRecord resolved_addr;
		ice::AddrRecord relayed_addr;
		turn::TurnMap turn_map;
		int64_t allocation_expiry = 0;
		int64_t permission_expiry = 0;
		uint16_t data_channel = 0;
		bool allocated = false;
	};

	Description::Role mRole;
	string mMid;
	std::atomic<GatheringState> mGatheringState;
	candidate_callback mCandidateCallback;
	gathering_state_callback mGatheringStateChangeCallback;

	ice::IceDescription mLocalDescription;
	ice::IceDescription mRemoteDescription;
	std::vector<ice::IceCandidate> mLocalCandidates;
	std::vector<ice::IceCandidate> mRemoteCandidates;
	std::vector<ice::IceCandidatePair> mPairs;
	ice::IceCandidatePair *mSelectedPair = nullptr;

	bool mIsControlling = false;
	uint64_t mTiebreaker = 0;

	std::vector<TurnCtx> mTurnServers;
	std::vector<IceCheck> mPendingChecks;

	std::unique_ptr<ace::net::net_interface> mNetIface;
	int mSocketFd = -1;
	std::mutex mSendMutex;

	std::atomic<bool> mRunning{true};
	Configuration mConfig;
};

} // namespace rtc::impl

#endif
