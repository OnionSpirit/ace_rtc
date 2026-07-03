/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 * Copyright (c) 2026 Ivan Moskalev (OnionSpirit)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// C++ API
#include "common.hpp"
#include "global.hpp"
//
#include "datachannel.hpp"
#include "iceudpmuxlistener.hpp"
#include "peerconnection.hpp"
#include "track.hpp"

namespace rtc {

// ACE event loop management.
// For ACE-native applications: call ace::run() in your main thread.
// For non-ACE applications: call rtc::Run() to start the event loop.
// rtc::RunAsync() starts ace::run() in a background thread.
// rtc::Stop() signals the event loop to terminate.

void Run();       // Blocking: calls ace::run() in the calling thread
void RunAsync();  // Non-blocking: spawns a thread with ace::run()
void Stop();      // Signals ace::terminate()

} // namespace rtc

#if RTC_ENABLE_WEBSOCKET

// WebSocket
#include "websocket.hpp"
#include "websocketserver.hpp"

#endif // RTC_ENABLE_WEBSOCKET

#if RTC_ENABLE_MEDIA

// Media
#include "av1rtppacketizer.hpp"
#include "dependencydescriptor.hpp"
#include "rtppacketizer.hpp"
#include "rtpdepacketizer.hpp"
#include "h264rtppacketizer.hpp"
#include "h264rtpdepacketizer.hpp"
#include "h265rtppacketizer.hpp"
#include "h265rtpdepacketizer.hpp"
#include "vp8rtppacketizer.hpp"
#include "vp8rtpdepacketizer.hpp"
#include "vp9rtppacketizer.hpp"
#include "vp9rtpdepacketizer.hpp"
#include "mediahandler.hpp"
#include "rtcpapphandler.hpp"
#include "plihandler.hpp"
#include "rembhandler.hpp"
#include "pacinghandler.hpp"
#include "rtcpnackresponder.hpp"
#include "rtcpreceivingsession.hpp"
#include "rtcpsrreporter.hpp"

#endif // RTC_ENABLE_MEDIA
