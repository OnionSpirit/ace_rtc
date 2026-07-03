# ACE Migration Roadmap

> **Project**: `ace_rtc` — fork of libdatachannel, migrated to ACE framework
> **Last updated**: 2026-07-03
> **Status**: I/O layer + Channel coroutine API done. PeerConnection/WS Server + ICE + tests pending.

---

## New Session Quickstart

**Build**: `meson setup build -Dno_tests=true -Dno_examples=true && meson compile -C build`

**Key architecture**: PIMPL pattern — `include/rtc/*.hpp` (public) wraps `src/impl/*.hpp` (internal). ACE runs the event loop — user calls `ace::run()` (ACE-native) or `rtc::RunAsync()` (background thread). All transport I/O (`TcpTransport`, `TlsTransport`, `DtlsTransport`, etc.) uses `ace::net` + `ace::schedule` internally. ThreadPool/PollService are vestigial (only in init.cpp).

**Current coroutine API** (on `Channel`, inherited by DataChannel/WebSocket/Track):
```cpp
ace::async<optional<message_variant>> msg = co_await channel.receive();
co_await channel.onOpen();
co_await channel.onClose();
co_await channel.onBufferedAmountLow();
string err = co_await channel.onError();
```

**impl::Channel has BOTH** legacy `synchronized_callback<>` (for C API + internal) + `ace::futures::channel<>` (for coroutine API). Trigger methods push into both. Cleanup target: remove legacy callbacks once C API is updated.

**PeerConnection** has ACE channels added (`stateAceChannel`, `iceStateAceChannel`, etc.) but public `onXxx()` methods still use legacy callbacks. Need to convert.

**Pending work**: see table below.

---

## Task Checklist

| # | Task | Status |
|---|------|--------|
| 1 | Transport I/O → ACE (Tcp, TLS, DTLS, WS, Processor) | ✅ Done |
| 2 | Channel coroutine API (receive/onOpen/onClose/onError) | ✅ Done |
| 3 | PeerConnection → convert onXxx() to ace::async<> | ✅ Done |
| 4 | WebSocketServer → convert onClient() to ace::async<> | ✅ Done |
| 5 | C API → removed entirely | ✅ Done |
| 6 | Remove legacy callbacks from impl::Channel | 🔴 Pending |
| 7 | ICE/libjuice → удалить, писать чистый ace::net ICE/STUN/TURN | 🔴 Pending |
| 8 | Fix examples (build) | ✅ Done |
| 9 | Convert tests + examples → coroutine API | 🔴 Pending |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     PUBLIC API (to migrate)                      │
│  PeerConnection  DataChannel  WebSocket  WebSocketServer  Track │
├─────────────────────────────────────────────────────────────────┤
│                    IMPL LAYER (migrated)                         │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Transport Stack (WebSocket path)                 │           │
│  │  WsTransport → TlsTransport → HttpProxyTransport │           │
│  │                    ↓                             │           │
│  │               TcpTransport                       │           │
│  │            (ace::net::connection)                 │           │
│  └──────────────────────────────────────────────────┘           │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Media/Data path                                  │           │
│  │  DtlsSrtpTransport → DtlsTransport               │           │
│  │  SctpTransport                                   │           │
│  │       ↓                                          │           │
│  │  IceTransport (libjuice — NOT yet migrated)      │           │
│  └──────────────────────────────────────────────────┘           │
├─────────────────────────────────────────────────────────────────┤
│                   ACE RUNTIME                                    │
│     ace::dispatcher  ace::net  ace::futures::channel             │
│     ace::futures::timeout  ace::spawn  ace::schedule             │
└─────────────────────────────────────────────────────────────────┘
```

## Event Loop Initialization

**ACE-native applications** (primary):
```cpp
#include <ace/ace.h>
#include <rtc/rtc.hpp>

int main() {
    rtc::Preload();        // optional: init non-ACE subsystems early
    // ... setup PeerConnection, etc ...
    ace::run();            // blocks — runs ACE + library event loop
}
```

**Non-ACE applications** (secondary):
```cpp
#include <rtc/rtc.hpp>

int main() {
    rtc::RunAsync();       // spawns ace::run() in background thread
    // ... use library normally ...
    rtc::Stop();           // signal shutdown
}
```

Or blocking:
```cpp
int main() {
    rtc::Run();            // blocks — ace::run() in calling thread
}
```

---

## Completed Migration (Increments 1-5)

### Increment 1: TcpTransport → ace::net
- **Files**: `src/impl/tcptransport.hpp`, `src/impl/tcptransport.cpp`
- **Changes**: Rewritten to use `ace::net::connection` (io_uring TCP). Uses `ace::futures::channel<message_ptr>` for sync→async bridge. Public interface (`start()`, `send()`, callbacks) preserved.
- **Key decisions**:
  - `unique_ptr<ace::net::connection>` (not `optional` — move semantics issue)
  - `ace::net::connection(fd, false)` constructor for passive sockets
  - `extract()` then `connection(fd, closed)` for active connections
  - `ace::futures::channel` for thread-safe send from sync `send()` to coroutine `sendLoop()`
  - No `ace/ace.h` in headers — specific includes only to avoid `operator&&` leakage

### Increment 2: HttpProxyTransport
- **Files**: none changed
- **Status**: No-op. HttpProxyTransport is a pure pass-through (delegates to TcpTransport). Already ACE-compatible.

### Increment 3: TlsTransport
- **Files**: `src/impl/tlstransport.cpp`
- **Changes**: `ThreadPool::Instance().enqueue(doRecv)` → `ace::schedule(doRecvTask)`. TLS internals (OpenSSL/GnuTLS/mbedTLS) unchanged.

### Increment 4: WebSocket layer timeouts
- **Files**: `src/impl/wstransport.cpp`, `src/impl/websocket.cpp`
- **Changes**: `ThreadPool::Instance().schedule(delay, callback)` → `ace::schedule(timeout_coroutine)` using `ace::futures::timeout`.

### Increment 5: Remaining ThreadPool → ACE
- **Files**: `src/impl/dtlstransport.cpp`, `src/impl/certificate.cpp`, `src/impl/logcounter.cpp`, `src/impl/processor.hpp`, `src/impl/processor.cpp`
- **Changes**: All `ThreadPool::enqueue()` and `ThreadPool::schedule()` replaced with `ace::schedule()` + `ace::futures::timeout()` / `ace::futures::expire()`.
- **certificate.cpp**: Uses `std::promise`/`std::shared_future` with `ace::schedule()` for async cert generation.

### Build system
- **Files**: `meson.build`, `meson_options.txt`
- C++23 standard
- All 18 CMake options preserved
- Wrap files for all subprojects: `ace`, `plog`, `usrsctp`, `libjuice`, `nlohmann_json`, `libsrtp2`
- `ace` dependency linked directly into libraries (not wrapped)
- `meson.override_dependency('ace_rtc', ace_rtc_dep)`

### Vestigial infrastructure
- `ThreadPool` and `PollService` still exist in `init.cpp` but no transport code uses them
- Can be removed once ICE is migrated

---

## Pending Migration

### Increment 7: ICE/libjuice → ace::net UDP

**Status**: ANALYZED. Decision: **remove libjuice entirely, use pure ace::net**.

**Rationale**: libjuice — C11 ICE/STUN/TURN library. Its C headers are incompatible with C++23. The road ahead:
1. Implement ICE agent directly on `ace::net::net_interface` (UDP sendto/recv via io_uring)
2. Implement STUN/TURN protocol in C++ (or port from libjuice's `stun.c`/`turn.c`)
3. Replace `IceTransport` → new `AceIceTransport` using pure ace::net
4. Remove `libjuice` subproject entirely

**Size**: ~8000 lines of protocol code to implement/port (ICE state machine, STUN binding, TURN relay, candidate gathering, SDP generation)

**Existing groundwork**:
- `deps/libjuice/src/conn_ace.cpp` — reference implementation of ACE I/O loop pattern
- `JUICE_CONCURRENCY_MODE_ACE` enum added to `juice.h`
- `conn_ace_registry_init()` completed — socket creation + net_interface wrapping pattern known
- Full analysis of libjuice internals completed (see commit history)

### Increment 7: Public API → coroutines

**Status**: DONE (hybrid approach). C API still uses legacy callbacks.

**Architecture**: impl::Channel keeps BOTH legacy `synchronized_callback<>` AND new `ace::futures::channel<>`. Public API exposes legacy bridge (for C API) + coroutine API (for C++ users). All transport/impl code unchanged — uses legacy callbacks internally.

**Channel** — coroutine API added:
- `ace::async<optional<message_variant>> receive()` — await next message
- `ace::async<> onOpen()`, `onClose()`, `onBufferedAmountLow()` — event streams
- `ace::async<string> onError()` — error stream
- Legacy bridge preserved: `onOpen(cb)`, `onClosed(cb)`, `onError(cb)`, `onMessage(cb)`, `onBufferedAmountLow(cb)`, `onAvailable(cb)`, `resetCallbacks()`, `receiveSync()`, `peek()`, `availableAmount()`

**DataChannel** — inherits coroutine API from Channel. No own changes needed.

**WebSocket** — inherits coroutine API from Channel. No own changes needed.

**WebSocketServer** — not yet converted. Still uses `onClient(cb)`.

**Track** — `flushPendingMessages()` pushes into `recvChannel` alongside legacy callbacks. `frameCallback` kept for internal use.

**PeerConnection** — ACE channels added for all events: `dataChannelAceChannel`, `localDescriptionAceChannel`, `localCandidateAceChannel`, `stateAceChannel`, `iceStateAceChannel`, `gatheringStateAceChannel`, `signalingStateAceChannel`, `trackAceChannel`. Legacy `synchronized_callback<>` members still present. Public API `onXxx()` methods NOT yet converted to `ace::async<>`.

### Remaining API work
- PeerConnection: convert `onStateChange(cb)` → `ace::async<State> stateChange()`, etc.
- WebSocketServer: convert `onClient(cb)` → `ace::async<shared_ptr<WebSocket>> acceptClient()`
- C API (capi.cpp): full rewrite to use ACE channels instead of callbacks
- Remove legacy callback infrastructure from impl::Channel (once C API is updated)

**Goal**: Convert all public-facing classes to coroutine-based API using `ace::async<>` and `ace::task`.

**Current callback-based API pattern**:
```cpp
pc->onStateChange([](State s) { ... });
pc->onLocalDescription([](Description d) { ... });
auto dc = pc->createDataChannel("label");
dc->send(data);
dc->onMessage([](message_variant msg) { ... });
```

**Target coroutine-based API pattern**:
```cpp
// Option A: async getters
auto state = co_await pc->stateChange();
auto desc = co_await pc->localDescription();
auto dc = pc->createDataChannel("label");
co_await dc->send(data);
auto msg = co_await dc->receiveMessage();

// Option B: generator-style
for co_await (auto msg : dc->messages()) { ... }
```

**Affected files** (public headers + impl):
- `include/rtc/peerconnection.hpp` + `src/peerconnection.cpp` + `src/impl/peerconnection.hpp/.cpp`
- `include/rtc/datachannel.hpp` + `src/datachannel.cpp` + `src/impl/datachannel.hpp/.cpp`
- `include/rtc/websocket.hpp` + `src/websocket.cpp` + `src/impl/websocket.hpp/.cpp`
- `include/rtc/websocketserver.hpp` + `src/websocketserver.cpp` + `src/impl/websocketserver.hpp/.cpp`
- `include/rtc/track.hpp` + `src/track.cpp` + `src/impl/track.hpp/.cpp`
- `include/rtc/channel.hpp` + `src/impl/channel.hpp/.cpp`

**Class hierarchy** (Cheshire Cat / PIMPL):
```
rtc::Channel (abstract)
  ├── rtc::DataChannel
  ├── rtc::WebSocket
  └── rtc::Track

rtc::PeerConnection
rtc::WebSocketServer
```

**Migration strategy**:
1. Start with `Channel` base — convert `send()` and callback registration to `ace::async<>` methods
2. Move to `DataChannel` (simplest) — convert all callbacks to awaitable methods
3. `WebSocket` — similar to DataChannel
4. `Track` — media-specific, similar to DataChannel with extra MediaHandler methods
5. `PeerConnection` — most complex (signaling, media, ICE, multiple callback types)
6. `WebSocketServer` — `onClient()` → `ace::async<shared_ptr<WebSocket>> acceptClient()`

**Key considerations**:
- Keep PIMPL pattern (CheshireCat<impl::T>)
- Use `ace::futures::channel<>` internally for callback→coroutine bridge
- `synchronized_callback<T>` should become `ace::futures::channel<T>` or similar
- Thread safety: ACE is single-threaded per runner, but public API may be called from any thread

### Increment 8: Fix unit tests + examples

**Status**: NOT STARTED.

**Test files** (`test/` directory):
- `test/main.cpp` — test runner entry point
- `test/connectivity.cpp` — WebRTC connectivity tests
- `test/negotiated.cpp` — negotiated DataChannel
- `test/reliability.cpp` — reliability modes
- `test/simulcast_sdp_*.cpp` — SDP generation/parsing
- `test/turn_connectivity.cpp` — TURN connectivity
- `test/track.cpp` — media track tests
- `test/video_layers_allocation.cpp`
- `test/capi_connectivity.cpp`, `test/capi_track.cpp`, `test/capi_websocketserver.cpp` — C API tests
- `test/websocket.cpp`, `test/websocketserver.cpp` — WebSocket tests
- `test/benchmark.cpp`
- `test/fir.cpp`, `test/rtx.cpp`, `test/rtcp_app.cpp` — media tests (only when RTC_ENABLE_MEDIA)

**Example files** (`examples/` directory):
- `examples/copy-paste/` — offerer/answerer (callback-based, need coroutine conversion)
- `examples/copy-paste-capi/` — C API example (needs rewrite after #5)
- `examples/client/` — WebRTC client
- `examples/client-benchmark/` — benchmark client
- `examples/media-sender/` — media sender
- `examples/media-receiver/` — media receiver
- `examples/media-sfu/` — media SFU
- `examples/signaling-server-qt/` — Qt signaling server
- `examples/streamer/` — media streamer

Tests and examples need updating after public API becomes coroutine-based. Will require:
1. Adding `ace::run()` call or `rtc::RunAsync()` in main
2. Converting test callbacks to coroutine awaits
3. Possibly restructuring test flow for coroutine-based API

---

## Key Technical Notes

### ACE `operator&&` leakage
ACE's `compose.h` redefines `operator and` (= `operator&&`) as a template for futures. This catches expressions like `optional<T> && bool` or `shared_ptr<T> && X`. **Never include `ace/ace.h` in headers** — use specific includes (`ace/core/dispatcher.h`, `ace/net.h`, `ace/futures/channel.h`, `ace/futures/timeout.h`). Do NOT include `ace/core/compose.h` unless absolutely needed.

### Move semantics with ace::net types
`ace::net::transport_entity` is move-only (copy deleted). `std::optional<T>` may not work with move-only types depending on GCC version. Use `std::unique_ptr<T>` instead. Construct from raw fd using `connection(int fd, bool is_closed)` or extract from another entity via `entity.extract()`.

### Thread safety
- `ace::futures::channel<T>::push()` is lock-free and thread-safe
- `ace::futures::channel<T>::pull()` is a coroutine awaitable
- Use channels to bridge sync (external thread) → async (ACE coroutine) communication
- `ace::schedule()` can be called from any thread

### Mutex replacement: ace::cutex
- **Prefer `ace::cutex` over `std::mutex`** for synchronization within ACE coroutine contexts
- `ace::cutex` is a cooperative mutex — it suspends the coroutine instead of blocking the thread
- **Constraint**: `ace::cutex::lock()` returns an awaitable. Can ONLY be used inside `ace::task`/`ace::async` coroutines via `co_await`
- Pattern:
  ```cpp
  ace::task critical_section() {
      auto guard = co_await mCutex.lock();
      // ... protected code ...
  } // guard releases on destruction
  ```
- For sync→coroutine bridges (external threads), keep `std::mutex` for the sync side
- Internal coroutine-to-coroutine synchronization should migrate to `ace::cutex`
- Relevant types: `ace::cutex`, `ace::guard` (RAII lock holder)

### C API (capi.cpp)
The C API wrappers will need updating after public C++ API changes. They expose wrapper functions for PeerConnection, DataChannel, Track, WebSocket, WebSocketServer with C-compatible callbacks.

---

## Meson Build Commands

```bash
# Configure and build (no tests/examples)
meson setup build -Dno_tests=true -Dno_examples=true
meson compile -C build

# Rebuild after changes
meson compile -C build

# Full clean rebuild
rm -rf build && meson setup build -Dno_tests=true -Dno_examples=true
```

---

## Subproject Dependencies

| Subproject | Type | Status |
|-----------|------|--------|
| `ace` | git wrap (OnionSpirit/ace) | Active, used for I/O |
| `usrsctp` | git wrap | Active, compiled as subproject |
| `libjuice` | git wrap (custom meson.build) | Active, compiled as subproject |
| `plog` | git wrap (custom meson.build) | Active, header-only |
| `nlohmann_json` | git wrap (built-in meson.build) | Header-only, for examples |
| `libsrtp2` | system pkg (fallback: wrap) | System version used |

---

## File Map: Migration-Affected Files

```
src/impl/
  tcptransport.hpp/cpp      ← REWRITTEN (ace::net)
  tlstransport.cpp          ← MODIFIED (ace::schedule)
  wstransport.cpp           ← MODIFIED (ace::futures::timeout)
  websocket.cpp             ← MODIFIED (ace::futures::timeout)
  dtlstransport.cpp         ← MODIFIED (ace::schedule + timeout/expire)
  certificate.cpp           ← MODIFIED (ace::schedule + promise)
  logcounter.cpp            ← MODIFIED (ace::futures::timeout)
  processor.hpp/cpp         ← MODIFIED (ace::schedule)
  init.cpp/hpp              ← MODIFIED (removed ACE thread, added public API)
  httpproxytransport.hpp/cpp ← UNCHANGED (pass-through, compatible)
  transport.hpp/cpp         ← UNCHANGED (base class, compatible)

include/rtc/
  rtc.hpp                   ← MODIFIED (added Run/RunAsync/Stop declarations)

src/
  global.cpp                ← MODIFIED (added Run/RunAsync/Stop implementations)

  (pending — ICE migration)
  impl/icetransport.hpp/cpp
  impl/iceudpmuxlistener.hpp/cpp

  (pending — public API migration)
  include/rtc/peerconnection.hpp
  include/rtc/datachannel.hpp
  include/rtc/websocket.hpp
  include/rtc/websocketserver.hpp
  include/rtc/track.hpp
  include/rtc/channel.hpp
  src/peerconnection.cpp
  src/datachannel.cpp
  src/websocket.cpp
  src/websocketserver.cpp
  src/track.cpp
  impl/peerconnection.hpp/cpp
  impl/datachannel.hpp/cpp
  impl/websocket.hpp/cpp
  impl/websocketserver.hpp/cpp
  impl/track.hpp/cpp
  impl/channel.hpp/cpp

meson.build                 ← REWRITTEN (c++23, ace dep, all options)
meson_options.txt           ← CREATED (18 options from CMake)
subprojects/*.wrap          ← CREATED (plog, usrsctp, libjuice, nlohmann_json, libsrtp2)
deps/plog/meson.build       ← CREATED (header-only wrapper)
deps/libjuice/meson.build   ← CREATED (full build wrapper)
```
