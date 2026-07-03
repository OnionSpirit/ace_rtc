project(
    'ace_rtc',
    'cpp',
    version: '0.24.5',
    default_options: ['cpp_std=c++23'],
    meson_version: '>=0.64.0',
)

# ============================================================================
# Resolve prefer_system_lib overrides (from CMakeLists.txt logic)
# ============================================================================
_prefer_system = get_option('prefer_system_lib')
_use_system_srtp = get_option('use_system_srtp') or _prefer_system
_use_system_juice = get_option('use_system_juice') or _prefer_system
_use_system_usrsctp = get_option('use_system_usrsctp') or _prefer_system
_use_system_plog = get_option('use_system_plog') or _prefer_system
_use_system_json = get_option('use_system_json') or _prefer_system

# Exclusive check: GnuTLS and MbedTLS cannot both be enabled
_use_gnutls = get_option('use_gnutls')
_use_mbedtls = get_option('use_mbedtls')
if _use_gnutls and _use_mbedtls
    error('Both use_mbedtls and use_gnutls cannot be enabled at the same time')
endif

_no_media = get_option('no_media')
_no_websocket = get_option('no_websocket')
_sctp_debug = get_option('sctp_debug')
_use_nice = get_option('use_nice')
_warnings_as_errors = get_option('warnings_as_errors')

# ============================================================================
# Source files (from CMakeLists.txt: LIBDATACHANNEL_SOURCES)
# ============================================================================
libdatachannel_sources = files(
    'src/candidate.cpp',
    'src/channel.cpp',
    'src/configuration.cpp',
    'src/datachannel.cpp',
    'src/dependencydescriptor.cpp',
    'src/description.cpp',
    'src/iceudpmuxlistener.cpp',
    'src/mediahandler.cpp',
    'src/global.cpp',
    'src/message.cpp',
    'src/peerconnection.cpp',
    'src/rtcpreceivingsession.cpp',
    'src/track.cpp',
    'src/websocket.cpp',
    'src/websocketserver.cpp',
    'src/rtppacketizationconfig.cpp',
    'src/video_layers_allocation.cpp',
    'src/rtcpsrreporter.cpp',
    'src/rtppacketizer.cpp',
    'src/rtpdepacketizer.cpp',
    'src/h264rtppacketizer.cpp',
    'src/h264rtpdepacketizer.cpp',
    'src/nalunit.cpp',
    'src/h265rtppacketizer.cpp',
    'src/h265rtpdepacketizer.cpp',
    'src/h265nalunit.cpp',
    'src/av1rtppacketizer.cpp',
    'src/vp8rtppacketizer.cpp',
    'src/vp8rtpdepacketizer.cpp',
    'src/vp9rtppacketizer.cpp',
    'src/vp9rtpdepacketizer.cpp',
    'src/rtcpnackresponder.cpp',
    'src/rtp.cpp',
    'src/plihandler.cpp',
    'src/pacinghandler.cpp',
    'src/rembhandler.cpp',
    'src/rtcpapphandler.cpp',
)

# ============================================================================
# Public headers (from CMakeLists.txt: LIBDATACHANNEL_HEADERS)
# ============================================================================
libdatachannel_headers = files(
    'include/rtc/candidate.hpp',
    'include/rtc/channel.hpp',
    'include/rtc/configuration.hpp',
    'include/rtc/datachannel.hpp',
    'include/rtc/dependencydescriptor.hpp',
    'include/rtc/description.hpp',
    'include/rtc/iceudpmuxlistener.hpp',
    'include/rtc/mediahandler.hpp',
    'include/rtc/rtcpreceivingsession.hpp',
    'include/rtc/common.hpp',
    'include/rtc/global.hpp',
    'include/rtc/message.hpp',
    'include/rtc/frameinfo.hpp',
    'include/rtc/peerconnection.hpp',
    'include/rtc/reliability.hpp',
    'include/rtc/rtc.hpp',
    'include/rtc/rtp.hpp',
    'include/rtc/track.hpp',
    'include/rtc/websocket.hpp',
    'include/rtc/websocketserver.hpp',
    'include/rtc/rtppacketizationconfig.hpp',
    'include/rtc/video_layers_allocation.hpp',
    'include/rtc/rtcpsrreporter.hpp',
    'include/rtc/rtppacketizer.hpp',
    'include/rtc/rtpdepacketizer.hpp',
    'include/rtc/h264rtppacketizer.hpp',
    'include/rtc/h264rtpdepacketizer.hpp',
    'include/rtc/nalunit.hpp',
    'include/rtc/h265rtppacketizer.hpp',
    'include/rtc/h265rtpdepacketizer.hpp',
    'include/rtc/h265nalunit.hpp',
    'include/rtc/av1rtppacketizer.hpp',
    'include/rtc/vp8rtppacketizer.hpp',
    'include/rtc/vp8rtpdepacketizer.hpp',
    'include/rtc/vp9rtppacketizer.hpp',
    'include/rtc/vp9rtpdepacketizer.hpp',
    'include/rtc/rtcpnackresponder.hpp',
    'include/rtc/utils.hpp',
    'include/rtc/plihandler.hpp',
    'include/rtc/pacinghandler.hpp',
    'include/rtc/rembhandler.hpp',
    'include/rtc/rtcpapphandler.hpp',
)

# ============================================================================
# Impl sources (from CMakeLists.txt: LIBDATACHANNEL_IMPL_SOURCES)
# ============================================================================
libdatachannel_impl_sources = files(
    'src/impl/certificate.cpp',
    'src/impl/channel.cpp',
    'src/impl/datachannel.cpp',
    'src/impl/dtlssrtptransport.cpp',
    'src/impl/dtlstransport.cpp',
    'src/impl/icetransport.cpp',
    'src/impl/iceudpmuxlistener.cpp',
    'src/impl/init.cpp',
    'src/impl/peerconnection.cpp',
    'src/impl/logcounter.cpp',
    'src/impl/sctptransport.cpp',
    'src/impl/threadpool.cpp',
    'src/impl/tls.cpp',
    'src/impl/track.cpp',
    'src/impl/utils.cpp',
    'src/impl/processor.cpp',
    'src/impl/sha.cpp',
    'src/impl/pollinterrupter.cpp',
    'src/impl/pollservice.cpp',
    'src/impl/http.cpp',
    'src/impl/httpproxytransport.cpp',
    'src/impl/tcpserver.cpp',
    'src/impl/tcptransport.cpp',
    'src/impl/tlstransport.cpp',
    'src/impl/transport.cpp',
    'src/impl/verifiedtlstransport.cpp',
    'src/impl/websocket.cpp',
    'src/impl/websocketserver.cpp',
    'src/impl/wstransport.cpp',
    'src/impl/wshandshake.cpp',
)

# ============================================================================
# Impl headers (from CMakeLists.txt: LIBDATACHANNEL_IMPL_HEADERS)
# ============================================================================
libdatachannel_impl_headers = files(
    'src/impl/certificate.hpp',
    'src/impl/channel.hpp',
    'src/impl/datachannel.hpp',
    'src/impl/dtlssrtptransport.hpp',
    'src/impl/dtlstransport.hpp',
    'src/impl/icetransport.hpp',
    'src/impl/iceudpmuxlistener.hpp',
    'src/impl/init.hpp',
    'src/impl/internals.hpp',
    'src/impl/peerconnection.hpp',
    'src/impl/queue.hpp',
    'src/impl/logcounter.hpp',
    'src/impl/sctptransport.hpp',
    'src/impl/threadpool.hpp',
    'src/impl/tls.hpp',
    'src/impl/track.hpp',
    'src/impl/utils.hpp',
    'src/impl/processor.hpp',
    'src/impl/sha.hpp',
    'src/impl/pollinterrupter.hpp',
    'src/impl/pollservice.hpp',
    'src/impl/http.hpp',
    'src/impl/httpproxytransport.hpp',
    'src/impl/tcpserver.hpp',
    'src/impl/tcptransport.hpp',
    'src/impl/tlstransport.hpp',
    'src/impl/transport.hpp',
    'src/impl/verifiedtlstransport.hpp',
    'src/impl/websocket.hpp',
    'src/impl/websocketserver.hpp',
    'src/impl/wstransport.hpp',
    'src/impl/wshandshake.hpp',
)

_all_sources = [
    libdatachannel_sources,
    libdatachannel_headers,
    libdatachannel_impl_sources,
    libdatachannel_impl_headers,
]

# ============================================================================
# Include directories
# ============================================================================
public_inc = include_directories('include')
private_inc = include_directories('include/rtc', 'src')

# ============================================================================
# Dependencies
# ============================================================================
thread_dep = dependency('threads')

# plog
if _use_system_plog
    plog_dep = dependency('plog', required: true)
else
    plog_dep = dependency('plog', fallback: ['plog', 'plog_dep'], required: true)
endif

# usrsctp
if _use_system_usrsctp
    usrsctp_dep = dependency('usrsctp', required: true)
else
    usrsctp_dep = dependency('usrsctp', fallback: ['usrsctp', 'usrsctp_dep'], required: true)
endif

# TLS backend
tls_dep = dependency('', required: false)
if _use_gnutls
    tls_dep = dependency('gnutls', required: true)
elif _use_mbedtls
    tls_dep = dependency('mbedtls', required: true)
else
    tls_dep = dependency('openssl', required: true)
endif

# Nettle (for WebSocket+SHA1 when using GnuTLS)
nettle_dep = dependency('', required: false)
if _use_gnutls and not _no_websocket
    nettle_dep = dependency('nettle', required: true)
endif

# libsrtp (media support)
srtp_dep = dependency('', required: false)
if not _no_media
    if _use_system_srtp
        srtp_dep = dependency('libsrtp2', required: true)
    else
        srtp_dep = dependency('libsrtp2', fallback: ['libsrtp2', 'libsrtp2_dep'], required: true)
    endif
    # libsrtp2 pkg-config on many systems does not export includedir; wrap for safety
    if srtp_dep.found() and srtp_dep.type_name() == 'pkgconfig'
        _srtp_inc = include_directories('/usr/include/srtp2')
        srtp_dep = declare_dependency(
            dependencies: srtp_dep,
            include_directories: _srtp_inc,
        )
    endif
endif

# ICE transport: libjuice or libnice
ice_dep = dependency('', required: false)
if _use_nice
    ice_dep = dependency('nice', required: true)
else
    if _use_system_juice
        ice_dep = dependency('libjuice', version: '>=1.7.0', required: true)
    else
        ice_dep = dependency('libjuice', fallback: ['libjuice', 'libjuice_dep'], required: true)
    endif
endif

# Windows winsock2
win32_dep = dependency('', required: false)
if host_machine.system() == 'windows'
    win32_dep = declare_dependency(
        link_args: ['-lws2_32'],
    )
endif

# ============================================================================
# Compile definitions
# ============================================================================

# Base private compile args for the library
lib_private_args = ['-DRTC_EXPORTS']

if _sctp_debug
    lib_private_args += ['-DSCTP_DEBUG']
endif

if get_option('build_shared_libs')
    lib_shared_extra_args = []
else
    lib_shared_extra_args = ['-DRTC_STATIC']
endif

# Public compile definitions (visible to consumers)
lib_public_args = []

if _no_websocket
    lib_public_args += ['-DRTC_ENABLE_WEBSOCKET=0']
else
    lib_public_args += ['-DRTC_ENABLE_WEBSOCKET=1']
endif

if _no_media
    lib_public_args += ['-DRTC_ENABLE_MEDIA=0']
else
    lib_public_args += ['-DRTC_ENABLE_MEDIA=1']
endif

# TLS compile definitions
if _use_gnutls
    lib_private_args += ['-DUSE_GNUTLS=1']
elif _use_mbedtls
    lib_private_args += ['-DUSE_MBEDTLS=1']
else
    lib_private_args += ['-DUSE_GNUTLS=0']
endif

# ICE compile definitions
if _use_nice
    lib_private_args += ['-DUSE_NICE=1']
else
    lib_private_args += ['-DUSE_NICE=0']
    if _use_system_juice
        lib_private_args += ['-DRTC_SYSTEM_JUICE=1']
    else
        lib_private_args += ['-DRTC_SYSTEM_JUICE=0']
    endif
endif

# Build shared libs option mirroring (CMake props: CXX_VISIBILITY_PRESET default)
if host_machine.system() == 'windows'
    if get_option('build_shared_libs')
        lib_private_args += ['-DDLL_EXPORTS']
    endif
endif

# SRTP compile definitions
private_srtp_args = []
if not _no_media
    if _use_system_srtp
        private_srtp_args = ['-DRTC_SYSTEM_SRTP=1']
    else
        private_srtp_args = ['-DRTC_SYSTEM_SRTP=0']
    endif
endif

# ============================================================================
# Compile options (from CMakeLists: -Wall -Wextra for non-MSVC)
# Must be before any build target
# ============================================================================
if host_machine.system() != 'windows'
    add_project_arguments('-Wall', '-Wextra', language: 'cpp')
endif
if _warnings_as_errors
    add_project_arguments('-Werror', language: 'cpp')
endif

# ============================================================================
# Library targets (from CMakeLists.txt: add_library(datachannel ...) + add_library(datachannel-static ...))
# ============================================================================

ace = dependency('ace')

# Shared library
lib_ace_rtc_shared = library(
    'ace_rtc',
    _all_sources,
    include_directories: [public_inc, private_inc],
    cpp_args: lib_private_args + lib_public_args + lib_shared_extra_args + private_srtp_args,
    dependencies: [
        ace,
        thread_dep,
        usrsctp_dep,
        plog_dep,
        tls_dep,
        nettle_dep,
        srtp_dep,
        ice_dep,
        win32_dep,
    ],
    version: meson.project_version(),
    soversion: '0',
    install: true,
)

ace_rtc_dep = declare_dependency(
    include_directories: public_inc,
    link_with: lib_ace_rtc_shared,
    compile_args: lib_public_args + lib_shared_extra_args,
    dependencies: ace,
)

meson.override_dependency('ace_rtc', ace_rtc_dep)

# ============================================================================
# Static library (from CMakeLists.txt: add_library(datachannel-static STATIC ...))
# ============================================================================
lib_ace_rtc_static = static_library(
    'ace_rtc_static',
    _all_sources,
    include_directories: [public_inc, private_inc],
    cpp_args: lib_private_args + lib_public_args + private_srtp_args,
    dependencies: [
        ace,
        thread_dep,
        usrsctp_dep,
        plog_dep,
        tls_dep,
        nettle_dep,
        srtp_dep,
        ice_dep,
        win32_dep,
    ],
    install: true,
)

# ============================================================================
# Install headers (from CMakeLists.txt: install(FILES ... DESTINATION ...))
# ============================================================================
install_headers(
    libdatachannel_headers,
    subdir: 'rtc',
)

# Export pkg-config file
pkg = import('pkgconfig')
pkg.generate(
    lib_ace_rtc_shared,
    name: 'ace_rtc',
    description: 'C/C++ WebRTC network library featuring Data Channels, Media Transport, and WebSockets',
    version: meson.project_version(),
)

# ============================================================================
# Tests (from CMakeLists.txt: datachannel-tests and datachannel-benchmark)
# ============================================================================
if not get_option('no_tests')
    test_sources = files(
        'test/main.cpp',
        'test/connectivity.cpp',
        'test/negotiated.cpp',
        'test/reliability.cpp',
        'test/simulcast_sdp_generation.cpp',
        'test/simulcast_sdp_parsing.cpp',
        'test/turn_connectivity.cpp',
        'test/track.cpp',
        'test/video_layers_allocation.cpp',
        'test/websocket.cpp',
        'test/websocketserver.cpp',
        'test/benchmark.cpp',
        'test/test.hpp',
    )

    if not _no_media
        test_sources += files(
            'test/fir.cpp',
            'test/rtx.cpp',
            'test/rtcp_app.cpp',
        )
    endif

    test_exe = executable(
        'ace_rtc_tests',
        test_sources,
        include_directories: private_inc,
        dependencies: [ace_rtc_dep],
        install: false,
    )

    test('ace_rtc_tests', test_exe)

    benchmark_exe = executable(
        'ace_rtc_benchmark',
        'test/benchmark.cpp',
        include_directories: private_inc,
        cpp_args: ['-DBENCHMARK_MAIN=1'],
        dependencies: [ace_rtc_dep],
        install: false,
    )

    benchmark('ace_rtc_benchmark', benchmark_exe)
endif

# ============================================================================
# Examples (from CMakeLists.txt)
# ============================================================================
if not get_option('no_examples')
    # nlohmann_json (from CMakeLists: JSON_BuildTests OFF)
    if _use_system_json
        json_dep = dependency('nlohmann_json', required: true)
    else
        json_dep = dependency('nlohmann_json', fallback: ['nlohmann_json', 'nlohmann_json_dep'], required: true)
    endif

    # ---- copy-paste (offerer + answerer) ----
    executable(
        'copy-paste-offerer',
        'examples/copy-paste/offerer.cpp',
        dependencies: [ace_rtc_dep],
        install: false,
    )
    executable(
        'copy-paste-answerer',
        'examples/copy-paste/answerer.cpp',
        dependencies: [ace_rtc_dep],
        install: false,
    )

    # ---- client (websocket) ----

    if not _no_websocket
        executable(
            'client',
            [
                'examples/client/main.cpp',
                'examples/client/parse_cl.cpp',
                'examples/client/parse_cl.h',
                'examples/client/getopt.cpp',
                'examples/client/getopt.h',
            ],
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )

        # ---- client-benchmark (websocket) ----
        executable(
            'client-benchmark',
            [
                'examples/client-benchmark/main.cpp',
                'examples/client-benchmark/parse_cl.cpp',
                'examples/client-benchmark/parse_cl.h',
                'examples/client-benchmark/getopt.cpp',
                'examples/client-benchmark/getopt.h',
            ],
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )
    endif

    # ---- media-receiver, media-sender, media-sfu ----
    if not _no_media
        executable(
            'media-receiver',
            'examples/media-receiver/main.cpp',
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )
        executable(
            'media-sender',
            'examples/media-sender/main.cpp',
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )
        executable(
            'media-sfu',
            'examples/media-sfu/main.cpp',
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )
    endif

    # ---- streamer (media + websocket) ----
    if not _no_media and not _no_websocket
        executable(
            'streamer',
            [
                'examples/streamer/main.cpp',
                'examples/streamer/dispatchqueue.cpp',
                'examples/streamer/dispatchqueue.hpp',
                'examples/streamer/h264fileparser.cpp',
                'examples/streamer/h264fileparser.hpp',
                'examples/streamer/helpers.cpp',
                'examples/streamer/helpers.hpp',
                'examples/streamer/opusfileparser.cpp',
                'examples/streamer/opusfileparser.hpp',
                'examples/streamer/fileparser.cpp',
                'examples/streamer/fileparser.hpp',
                'examples/streamer/stream.cpp',
                'examples/streamer/stream.hpp',
                'examples/streamer/ArgParser.cpp',
                'examples/streamer/ArgParser.hpp',
            ],
            dependencies: [ace_rtc_dep, json_dep],
            install: false,
        )
    endif
endif
