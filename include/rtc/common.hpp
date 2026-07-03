/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_COMMON_H
#define RTC_COMMON_H

#ifdef RTC_STATIC
#define RTC_CPP_EXPORT
#else // dynamic library
#ifdef _WIN32
#ifdef RTC_EXPORTS
#define RTC_CPP_EXPORT __declspec(dllexport) // building the library
#else
#define RTC_CPP_EXPORT __declspec(dllimport) // using the library
#endif
#else // not WIN32
#define RTC_CPP_EXPORT
#endif
#endif

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 // Windows 8
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4251) // disable "X needs to have dll-interface..."
#endif
#endif

#ifndef RTC_ENABLE_WEBSOCKET
#define RTC_ENABLE_WEBSOCKET 1
#endif

#ifndef RTC_ENABLE_MEDIA
#define RTC_ENABLE_MEDIA 1
#endif

// PeerConnection state constants (used by enum class initializers)
#define RTC_NEW 0
#define RTC_CONNECTING 1
#define RTC_CONNECTED 2
#define RTC_DISCONNECTED 3
#define RTC_FAILED 4
#define RTC_CLOSED 5

#define RTC_ICE_NEW 0
#define RTC_ICE_CHECKING 1
#define RTC_ICE_CONNECTED 2
#define RTC_ICE_COMPLETED 3
#define RTC_ICE_FAILED 4
#define RTC_ICE_DISCONNECTED 5
#define RTC_ICE_CLOSED 6

#define RTC_GATHERING_NEW 0
#define RTC_GATHERING_INPROGRESS 1
#define RTC_GATHERING_COMPLETE 2

#define RTC_SIGNALING_STABLE 0
#define RTC_SIGNALING_HAVE_LOCAL_OFFER 1
#define RTC_SIGNALING_HAVE_REMOTE_OFFER 2
#define RTC_SIGNALING_HAVE_LOCAL_PRANSWER 3
#define RTC_SIGNALING_HAVE_REMOTE_PRANSWER 4

// Certificate type
#define RTC_CERTIFICATE_DEFAULT 0
#define RTC_CERTIFICATE_ECDSA 1
#define RTC_CERTIFICATE_RSA 2

// Transport policy
#define RTC_TRANSPORT_POLICY_ALL 0
#define RTC_TRANSPORT_POLICY_RELAY 1

// Direction
#define RTC_DIRECTION_UNKNOWN 0
#define RTC_DIRECTION_SENDONLY 1
#define RTC_DIRECTION_RECVONLY 2
#define RTC_DIRECTION_SENDRECV 3
#define RTC_DIRECTION_INACTIVE 4

// NAL unit separator
#define RTC_NAL_SEPARATOR_LENGTH 0
#define RTC_NAL_SEPARATOR_LONG_START_SEQUENCE 1
#define RTC_NAL_SEPARATOR_SHORT_START_SEQUENCE 2
#define RTC_NAL_SEPARATOR_START_SEQUENCE 3

// OBU packetization
#define RTC_OBU_PACKETIZED_OBU 0
#define RTC_OBU_PACKETIZED_TEMPORAL_UNIT 1

// MTU and fragment size
#define RTC_DEFAULT_MTU 1280
#if RTC_ENABLE_MEDIA
#define RTC_DEFAULT_MAX_FRAGMENT_SIZE ((uint16_t)(RTC_DEFAULT_MTU - 12 - 8 - 40))
#endif

#include "utils.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rtc {

using std::byte;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::variant;
using std::weak_ptr;

using binary = std::vector<byte>;
using message_variant = variant<binary, string>;

using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
using std::ptrdiff_t;
using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

} // namespace rtc

#endif
