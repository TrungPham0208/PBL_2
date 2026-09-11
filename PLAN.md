# Implementation Plan

This plan follows `SPEC.md`. Each milestone is labeled by review depth:

- **Core logic**: requires a deep checkpoint because it controls protocol behavior, data-structure mechanics, or a stated invariant/security/economic property.
- **Supporting/boilerplate**: requires a quick review for wiring, presentation, or project setup.

## Milestones

### 1. Core logic — Custom data-structure foundations

Start with the smallest useful milestone for the first teacher review: implement and test the required structures ourselves before building routing or simulation services. Provide focused, readable implementations of a dynamic sequence/storage base as needed, FIFO `queue`, binary-heap `priority_queue`, associative `map`/hash-map structure, and an adjacency-list structure. Do not use `std::queue`, `std::priority_queue`, `std::map`, or `std::unordered_map` for the core simulator behavior. Add the CMake/test skeleton and direct unit tests for insertion, lookup, removal, ordering, capacity/error behavior, and empty-structure behavior.

Preserve these properties:

- `queue` is FIFO and never removes an item out of order.
- `priority_queue` returns the highest-priority item first and preserves FIFO order for equal priorities where the simulator requires it.
- `map`/hash-map lookup, update, and removal maintain one authoritative value per key.
- The adjacency list represents each undirected connection consistently from both endpoints and does not create duplicate or dangling entries.
- Core behavior does not silently fall back to an STL container with different semantics.
- Bounds and invalid operations are handled explicitly instead of causing silent data loss.
- These data structure are implemented such that, i can call and reuse them quickly and easily everywhere in this project. 

Review checkpoint: this is the first teacher checkpoint. Be able to draw each structure, explain its stored state and main operations, state its time complexity at a beginner-appropriate level, and demonstrate the tests. Explain any templates, references, ownership, or dynamic allocation before using them.

### 2. Core logic — Basic OOP model and focused tests

Implement `Node`, `Client`, `Router`, `Packet`, and `Link` using the custom structures where applicable. Verify construction, identity, active/failed state, packet metadata, and link configuration.

Preserve these properties:

- Nodes have stable identifiers and an explicit active/failed state.
- Links remain undirected with one shared status in both directions.
- Link configuration uses simulated bandwidth, latency, loss, and corruption values only; no real network access is introduced.
- Packet metadata is explicit and sufficient to distinguish DATA, ACK, route-probe, heartbeat, and route-update behavior.

Review checkpoint: explain inheritance, composition, ownership, and why model classes do not depend on Qt widgets.

### 3. Core logic — Network graph and validation

Build `NetworkGraph` on the custom adjacency-list and map implementations. Add node/link operations, active-state handling, and validation for isolated nodes, unreachable pairs, and missing alternative routes. Add `test_graph.cpp`.

Preserve these properties:

- Every undirected link is represented consistently from both endpoints.
- Failed nodes and links cannot be treated as usable routing connections.
- Validation reports invalid topology conditions without silently repairing or changing the user’s topology.
- Graph operations do not create duplicate or dangling connections.

Review checkpoint: trace one link insertion, removal, failure, and validation result through the custom adjacency list.

### 4. Core logic — Dijkstra routing

Implement `RoutingService` with the custom priority queue and one centralized, documented non-negative cost formula using latency, bandwidth, and loss probability. Add `test_routing.cpp`.

Preserve these properties:

- Dijkstra never selects failed nodes or links.
- Route costs are non-negative and use the documented formula consistently.
- The selected route is the lowest-cost available route; unreachable destinations are reported explicitly.
- Route calculation does not mutate packet-transfer state.

Review checkpoint: explain the priority queue, stale entries, route reconstruction, and why each metric affects the score.

### 5. Core logic — Queues and fixed-tick simulation

Implement bounded link queues, priority scheduling, transmission-time calculation, and `SimulationService` with the fixed `1 ms` tick. Add tests for queue priority, same-priority FIFO behavior, capacity, latency, and bandwidth timing.

Preserve these properties:

- `ACK` and `ROUTE_PROBE` outrank `HEARTBEAT` and `ROUTE_UPDATE`, which outrank `DATA`.
- Packets with equal priority remain FIFO.
- Queues never exceed their configured bound.
- Timing uses configured model values and `ceil(packet size / bandwidth)`; no real resource measurement occurs.
- Packets affected by a failed node or link are dropped rather than moved invisibly.

Review checkpoint: explain how one tick changes queue and packet state while keeping the simulation deterministic.

### 6. Core logic — Chunking, checksums, and reconstruction

Implement file chunking, sequence tracking, ordered chunk storage, XOR checksums, and reconstruction in `TransferService`. Add focused transfer tests.

Preserve these properties:

- Each chunk has stable sequence information and can be placed in order even when received out of order.
- Reconstruction returns the original bytes only when all required chunks are present and valid.
- A checksum mismatch rejects DATA and cannot produce a successful ACK.
- XOR is treated as an educational integrity check, never as cryptographic protection.

Review checkpoint: explain ordered storage, checksum calculation, missing chunks, and why CRC-32 is documented but not implemented.

### 7. Core logic — ACKs, timeout, retransmission, and retry limits

Add individual ACK tracking, the `100 ms` timeout, retransmission scheduling, the maximum of `3` attempts, and transfer success/failure statistics.

Preserve these properties:

- Only a valid ACK for the matching chunk can mark that chunk delivered.
- A lost, corrupted, or missing DATA packet remains unacknowledged.
- Acknowledged chunks are not retransmitted unnecessarily after a route failure.
- The transfer fails after the final allowed attempt and preserves evidence in statistics.

Review checkpoint: walk through one success, one timeout/retry, and one retry-limit failure.

### 8. Core logic — Loss, corruption, and heartbeat failure detection

Implement deterministic manual injection plus configured loss/corruption behavior, heartbeat monitoring, failure timeouts, and failure events.

Preserve these properties:

- Default zero probabilities remain deterministic.
- Corruption is detected before successful delivery acknowledgment.
- Heartbeat failure changes node/link availability only after the documented timeout, unless a manual failure is requested.
- Failure detection does not claim production reliability or security.

Review checkpoint: distinguish packet loss, packet corruption, manual failure, and heartbeat timeout in the event log.

### 9. Core logic — Rerouting and recovery

Connect failure handling to graph updates, Dijkstra recalculation, affected-packet dropping, and retransmission of missing chunks over the replacement route. Add `test_failure_recovery.cpp`.

Preserve these properties:

- A failed node or link is removed from the usable route immediately.
- In-transit packets are dropped, never magically moved to a new route.
- Acknowledged chunks remain acknowledged; only missing chunks are retried.
- Recovery succeeds when an alternate route exists and reports failure when none exists.

Review checkpoint: explain the complete recovery sequence from failure detection to resumed transfer.

### 10. Core logic — JSON configuration and experiment persistence

Implement `FileStorageService` for `config/config.json` and experiment result JSON containing configuration, packet events, route changes, timing, outcome, and statistics.

Preserve these properties:

- Stored configuration can reproduce the same topology and configured experiment values.
- Persistence is local, file-based, and JSON-only; no database, paid service, credential, or external service is added.
- Invalid or incomplete JSON is rejected clearly rather than silently producing unsafe defaults.
- Saved results describe what happened without changing simulation behavior.

Review checkpoint: explain the JSON schema and how a saved seed/configuration reproduces a demonstration.

### 11. Supporting/boilerplate — Qt dashboard and topology controls

Build the Qt6 `MainWindow` and presentation wiring for topology editing, configuration, simulation controls, route/log/queue displays, transfer progress, and statistics. Keep all network rules in model/services.

Quick review:

- UI code follows the specified layout and does not contain Dijkstra, retry, checksum, failure, or persistence algorithms.
- Start, pause, resume, reset, and single-tick controls call the simulation service.
- Input validation reports problems without silently changing core state.

### 12. Supporting/boilerplate — Integration and coherent demonstration

Wire the completed services into the documented end-to-end scenario: topology generation, probe, route selection, transfer, injected failure, reroute, recovery, result save, and controlled retry-limit failure.

Quick review:

- The demonstration is reproducible with a saved seed and deterministic defaults.
- Logs make route changes, packet outcomes, ACKs, retries, and final status visible.
- The UI remains a presentation layer over tested core behavior.

### 13. Supporting/boilerplate — Documentation and final verification

Add or update `README.md` with build, test, run, demonstration, limitations, and beginner-C++ notes. Run the complete relevant test suite and verify the two demonstration outcomes.

Quick review:

- Documentation states that this is a model-only educational simulator.
- Limitations include no real sockets, no production security/reliability, and XOR’s non-cryptographic nature.
- Test results and remaining risks are recorded before completion is claimed.
