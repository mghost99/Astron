# Boost.Asio Networking Overview

Astron now runs entirely on Boost.Asio. This note captures the new structure so
future work can focus on higher-level behaviour rather than event-loop plumbing.

## Core runtime
- `src/util/NetContext.*` owns the single `boost::asio::io_context`, exposes
  `run/stop/start/join`, and replaces the old `g_loop`.
- `src/util/TaskQueue.*` posts callbacks back onto the io_context so the main
  thread continues to service asynchronous completions deterministically.
- `src/core/main.cpp` initializes `NetContext`, records `g_main_thread_id`, and
  blocks in `NetContext::run()` until shutdown. `astron_shutdown()` now stops the
  context and joins any background threads.

## Networking primitives (`src/net`)
- `NetworkClient.*`: async TCP client using `boost::asio::ip::tcp::socket`
  + `steady_timer` for write deadlines. HAProxy parsing now emits `NetAddress`
  and `NetErrorEvent` surfaces Boost error codes.
- `NetworkAcceptor.*` / `TcpAcceptor.*`: inbound listeners on `boost::asio`
  acceptors returning shared TCP sockets plus resolved endpoint metadata.
- `NetworkConnector.*`: outbound connector that resolves hostnames with
  Asio’s resolver and feeds sockets into higher layers.
- `address_utils.*`: uses Boost.Asio resolvers and `NetAddress` for parsing.
- `HAProxyHandler.*`: fully Boost-based address parsing with rich error codes.
- `NetTypes.h`: centralizes `NetAddress`, `NetErrorEvent`, and socket aliases.

## Messaging roles
- `MessageDirector.*`, `MDNetworkParticipant.*`, `MDNetworkUpstream.*` all use
  the new socket types. Cleanup timers now run via `boost::asio::steady_timer`.
- Disconnect/error paths propagate `NetErrorEvent` so logs include Asio messages.

## Client agent
- `ClientAgent`, `ClientFactory`, and `AstronClient` now consume
  `TcpSocketPtr`/`NetAddress`. Heartbeat timers run on the shared io_context and
  all disconnect handlers understand `NetErrorEvent`.

## Event pipeline
- `EventSender`: UDP telemetry built on `boost::asio::ip::udp::socket`, with
  sends marshalled through the TaskQueue.
- `EventLogger`: UDP listener using Boost sockets plus async receive loops.
- `Timeout`: portable wrapper around `steady_timer`, still self-destructing when
  fired/cancelled to preserve existing semantics.

## Remaining cleanups
- `deps/uvw` and `cmake/modules/Findlibuv.cmake` can be deleted once downstream
  consumers confirm they no longer depend on libuv.
- Documentation and developer guides should be updated to reference NetContext
  and the new Asio-based threading model.

