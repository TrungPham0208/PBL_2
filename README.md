# Fault-Tolerant File Transmission and Routing Simulator
<!-- Milestone 13: Supporting/boilerplate — Documentation and final verification -->

An educational computer network simulator written in **C++20** with a **Qt6** graphical dashboard, developed for the **PBL2 (Project-Based Learning 2)** curriculum. The project demonstrates core principles of Object-Oriented Programming (OOP), Data Structures and Algorithms (DSA), packet transmission, dynamic failure detection, Dijkstra-based rerouting, and local file persistence.

> [!IMPORTANT]
> **Model-Only Educational Simulator**: This software is strictly a deterministic, model-only simulation. It does **not** use real TCP or UDP network sockets, does **not** interact with the host operating system's network stack, and does **not** provide cryptographic security or production reliability guarantees. Bandwidth, latency, packet loss, and corruption are simulated parameters evaluated on a fixed 1 ms clock tick.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Academic & Technical Highlights](#academic--technical-highlights)
3. [Architecture and Repository Structure](#architecture-and-repository-structure)
4. [Beginner C++ Conventions & Pedagogical Notes](#beginner-c-conventions--pedagogical-notes)
5. [Combined Routing Formula](#combined-routing-formula)
6. [Coherent Demonstration Scenarios](#coherent-demonstration-scenarios)
7. [Building and Running](#building-and-running)
8. [Test Suite & Verification Results](#test-suite--verification-results)
9. [Persistence & JSON Schemas](#persistence--json-schemas)
10. [Limitations & Risk Assessment](#limitations--risk-assessment)

---

## 1. Project Overview

The simulator models an interconnected network of **Clients** and **Routers** connected by undirected, bidirectional **Links**. A source client transfers a file to a destination client by dividing the file into discrete data chunks, transmitting them through simulated link queues, verifying data integrity using checksums, and handling acknowledgements (ACKs) and timeouts.

When network anomalies (link failure, router crash, packet loss, or corruption) occur, the simulator detects the fault via periodic **heartbeats**, drops affected in-flight packets, dynamically recalculates the optimal alternative path using **Dijkstra's algorithm**, and retransmits unacknowledged chunks over the replacement route until the file is reconstructed 100% byte-for-byte or a controlled retry limit is exceeded.

---

## 2. Academic & Technical Highlights

- **Object-Oriented Design (OOP)**: Clear hierarchy with `Node` base class, specialized `Client` and `Router` entities, and encapsulated domain classes (`Packet`, `Link`, `NetworkGraph`).
- **Custom Data Structures (No STL in Core Mechanics)**: All fundamental collections used in core simulation are implemented from scratch:
  - `pbl::dsa::DynamicArray<T>`: Contiguous dynamic array with geometric growth and bounds checking.
  - `pbl::dsa::Queue<T>`: Bounded/unbounded FIFO circular array queue.
  - `pbl::dsa::PriorityQueue<T, Compare>`: Binary min/max heap supporting Dijkstra search and link priority scheduling.
  - `pbl::dsa::HashMap<K, V>`: Separate-chaining hash map for node lookups, chunk tracking, and neighbor mapping.
  - `pbl::dsa::AdjacencyList`: Graph edge representation storing undirected links.
- **Priority-Aware Link Queuing**: Packets are scheduled by priority: high-priority control packets (ACKs, Route Probes, Heartbeats) are processed ahead of bulk DATA packets.
- **Fixed-Tick Simulation Model**: The simulation clock advances in deterministic 1 ms discrete steps, calculating transmission delay ($\lceil \text{Size} / \text{Bandwidth} \rceil$) and propagation latency.
- **Reliable File Transfer Protocol**:
  - File chunking (configurable default 1024 bytes).
  - Sequence tracking and reassembly buffer.
  - Individual acknowledgements (ACKs).
  - Timeout detection (default 100 ms) and automatic retransmissions (up to 3 attempts).
  - Non-cryptographic XOR checksum verification on each chunk.
- **Fault Detection & Dynamic Rerouting**:
  - Periodic heartbeat ping-pong monitoring (100 ms interval, 300 ms failure timeout).
  - Immediate topology invalidation upon confirmed node/link failure.
  - Real-time Dijkstra route recalculation over surviving active nodes.
  - Mid-transfer packet dropping on broken links and seamless retransmission over replacement routes.
- **Local JSON Persistence**: Dependency-free recursive JSON parser and serializer for topology configurations (`config/config.json`) and detailed experiment logs (`data/experiments/`).
- **Interactive Qt6 GUI**: Visual canvas displaying nodes, links, active routes, queue occupancy, real-time packet movement, transmission progress, and an event log.

---

## 3. Architecture and Repository Structure

The project strictly follows the **one class per file** rule and maintains a clean separation between domain models, algorithms, services, and the presentation layer:

```
PBL_2/
├── CMakeLists.txt              # Unified build configuration (C++20, Qt6, CTest)
├── SPEC.md                     # Complete project specification and constraints
├── PLAN.md                     # Step-by-step milestone implementation plan
├── AGENTS.md                   # Beginner C++ coding rules and constraints
├── README.md                   # Milestone 13 documentation & verification
├── data/
│   └── experiments/            # Saved demonstration experiment outputs
│       ├── demonstration_recovery.json
│       └── demonstration_failure.json
├── include/
│   ├── pbl/
│   │   ├── dsa/                # Custom Data Structure Templates (Milestone 1)
│   │   │   ├── DynamicArray.h
│   │   │   ├── Queue.h
│   │   │   ├── PriorityQueue.h
│   │   │   ├── HashMap.h
│   │   │   └── AdjacencyList.h
│   │   ├── model/              # Domain Entity Models (Milestones 2-3)
│   │   │   ├── Node.h
│   │   │   ├── Client.h
│   │   │   ├── Router.h
│   │   │   ├── Packet.h
│   │   │   ├── Link.h
│   │   │   └── NetworkGraph.h
│   │   └── service/            # Core Simulation & Network Services (Milestones 4-10)
│   │       ├── RoutingService.h
│   │       ├── SimulationService.h
│   │       ├── TransferService.h
│   │       └── FileStorageService.h
│   └── ui/                     # Presentation Layer (Milestone 11)
│       ├── TopologyCanvas.h
│       └── MainWindow.h
├── src/
│   ├── model/                  # Domain Model Implementations
│   │   ├── Node.cpp
│   │   ├── Client.cpp
│   │   ├── Router.cpp
│   │   ├── Packet.cpp
│   │   ├── Link.cpp
│   │   └── NetworkGraph.cpp
│   ├── services/               # Core Services Implementations
│   │   ├── RoutingService.cpp
│   │   ├── SimulationService.cpp
│   │   ├── TransferService.cpp
│   │   └── FileStorageService.cpp
│   ├── ui/                     # Qt6 GUI Implementations
│   │   ├── TopologyCanvas.cpp
│   │   └── MainWindow.cpp
│   └── main.cpp                # GUI Application Entry Point
└── tests/                      # Comprehensive Unit & Integration Test Suites
    ├── test_data_structures.cpp
    ├── test_model.cpp
    ├── test_graph.cpp
    ├── test_routing.cpp
    ├── test_simulation.cpp
    ├── test_transfer.cpp
    ├── test_failure_recovery.cpp
    ├── test_storage.cpp
    ├── test_ui_wiring.cpp
    └── test_demonstration.cpp
```

> [!NOTE]
> **Presentation Layer Boundary**: The UI classes (`MainWindow` and `TopologyCanvas`) are purely graphical views. Neither class contains Dijkstra route calculation, chunking logic, retry timers, checksum algorithms, failure timeouts, or JSON parsing. All business logic resides in `src/services/` and `src/model/`.

---

## 4. Beginner C++ Conventions & Pedagogical Notes

In accordance with `AGENTS.md`, the codebase is specifically tailored for students and instructors in introductory object-oriented programming:

1. **Normalized Standard Header**: Headers and implementation files utilize `#include <bits/stdc++.h>` and `using namespace std;` to reduce include clutter and maintain focus on algorithms rather than header management.
2. **Standard Types Only**: Unsigned integer types such as `size_t` are avoided in favor of standard `int` and `double` to prevent unsigned underflow bugs and complex type casts.
3. **No Forbidden Advanced Syntax**:
   - No `explicit` keyword on single-argument constructors.
   - No constructor member initializer lists (`: member(...)`). Member initialization is written explicitly inside constructor function bodies using clean, observable assignment statements.
   - No deleted copy constructors or deleted assignment operators (`= delete`).
   - No operator overloading purely for guard syntax.
4. **Explicit Resource Ownership & Destructors**:
   - Destructors are implemented **only** when a class directly manages raw heap memory (e.g., dynamically allocated buffer arrays in `DynamicArray`, `Queue`, and `HashMap`).
   - In classes that do not own raw heap memory, destructors are omitted or defaulted, and ownership is documented in plain comments.
5. **Careful Pointer Usage**:
   - Pointers are reserved for Qt parent-child hierarchy requirements and polymorphic graph node references. Everywhere else, direct values and references are preferred.

---

## 5. Combined Routing Formula

Routing paths are computed using Dijkstra's shortest path algorithm over an adjacency list using our custom `PriorityQueue`. Link costs are calculated centrally in `RoutingService::calculate_link_cost` using the following non-negative formula:

$$\text{Cost} = \text{Latency (ms)} + \frac{1000.0}{\text{Bandwidth (bytes/ms)}} + 50.0 \times \text{Loss Rate}$$

- **Latency** provides the base delay in milliseconds.
- **Bandwidth** contributes inverse transmission delay (higher bandwidth reduces cost).
- **Packet Loss Rate** ($0.0 \dots 1.0$) penalizes unreliable links with an additive penalty.
- Inactive or failed links and links connected to failed nodes return an infinite cost ($10^9$), causing Dijkstra to bypass them entirely.

---

## 6. Coherent Demonstration Scenarios

The simulator provides two fully reproducible end-to-end scenarios (executable via `pbl2_simulator.exe` or validated through `tests/test_demonstration.cpp`):

### Scenario 1: Failure Recovery & Dynamic Rerouting (`demonstration_recovery.json`)
- **Topology Setup**: Seed `12345` generates a connected 6-node network containing Client 1, Client 2, and Routers 3, 4, 5, 6.
- **Route Probe**: Client 1 dispatches a hop-by-hop `ROUTE_PROBE` to Client 2. The discovered shortest path is `1 -> 4 -> 2`.
- **Transfer Start**: Client 1 initiates a 4-chunk transfer (128 bytes total). In-flight chunks 0, 1, 2, and 3 are transmitted onto Link `1-4`.
- **Mid-Transfer Failure**: Router 4 is manually marked failed at tick 65 ms.
- **Failure Dropping**: Packets buffered in or traveling through Router 4 are dropped immediately.
- **Reroute & Retransmit**: The routing service discovers surviving alternate route `1 -> 3 -> 6 -> 5 -> 2`. Unacknowledged chunks 0, 1, 2, 3 time out and are retransmitted over the new path.
- **Successful Reconstruction**: Receiver Client 2 validates individual XOR checksums, returns ACKs, and reconstructs the payload with **100% byte-for-byte integrity**.
- **Evidence**: Stored in `data/experiments/demonstration_recovery.json`.

### Scenario 2: Controlled Retry-Limit Failure (`demonstration_failure.json`)
- **Isolation Event**: All links connecting destination Client 2 are severed.
- **Attempt Tracking**: Sender Client 1 transmits chunk 0. No ACK returns before the 100 ms timeout.
- **Retry Progression**: Chunk 0 is retransmitted on Attempt 2 (tick 101 ms) and Attempt 3 (tick 201 ms).
- **Limit Exceeded**: Upon the 3rd consecutive timeout, the retransmission limit is exhausted. The transfer transitions to `FAILED`.
- **Evidence**: Stored in `data/experiments/demonstration_failure.json` recording 3 timeouts, 2 retransmissions, and final outcome `FAILURE`.

---

## 7. Building and Running

### Prerequisites
- **Compiler**: GCC 13+ (MinGW-w64 64-bit) supporting C++20.
- **Build System**: CMake 3.16+ and Ninja.
- **UI Framework**: Qt 6.8+ (MinGW 64-bit).

### Build Commands (Windows PowerShell)

```powershell
# 1. Set environment path to MinGW and Qt6 binaries
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.8.0\mingw_64\bin;$env:PATH"

# 2. Configure the project using CMake and Ninja
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" -B build -G "Ninja" `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/mingw_64" `
    -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"

# 3. Compile all libraries, executables, and tests
& "C:\Qt\Tools\Ninja\ninja.exe" -C build
```

### Running Unit and Integration Tests

```powershell
& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build --output-on-failure
```

### Launching the Interactive GUI Application

```powershell
.\build\pbl2_simulator.exe
```

---

## 8. Test Suite & Verification Results

The test suite consists of 10 automated CTest suites testing all layers from custom data structures to GUI signal wiring and end-to-end demonstrations:

| Test Name | Source File | Description | Status |
| :--- | :--- | :--- | :---: |
| **`pbl2_data_structures`** | `tests/test_data_structures.cpp` | DynamicArray, Queue, PriorityQueue, HashMap, AdjacencyList | **PASSED** |
| **`pbl2_model`** | `tests/test_model.cpp` | Node, Client, Router, Packet, Link construction and state | **PASSED** |
| **`pbl2_graph`** | `tests/test_graph.cpp` | NetworkGraph topology editing, validation, connectivity checks | **PASSED** |
| **`pbl2_routing`** | `tests/test_routing.cpp` | Centralized cost formula, Dijkstra shortest path calculation | **PASSED** |
| **`pbl2_simulation`** | `tests/test_simulation.cpp` | 1 ms tick engine, priority link queues, probe packet dispatch | **PASSED** |
| **`pbl2_transfer`** | `tests/test_transfer.cpp` | Chunking, XOR checksums, individual ACKs, byte-exact reconstruction | **PASSED** |
| **`pbl2_failure_recovery`** | `tests/test_failure_recovery.cpp` | Loss/corruption rejection, heartbeat failure detection, rerouting | **PASSED** |
| **`pbl2_storage`** | `tests/test_storage.cpp` | Custom JSON parser/serializer for config and experiment output | **PASSED** |
| **`pbl2_ui`** | `tests/test_ui_wiring.cpp` | Qt MainWindow controls, canvas rendering, simulation step wiring | **PASSED** |
| **`pbl2_demonstration`** | `tests/test_demonstration.cpp` | End-to-end Scenarios 1 & 2 verification, JSON evidence output | **PASSED** |

**CTest Execution Summary**:
```
100% tests passed, 0 tests failed out of 10
Total Test time (real) = 0.18 sec
```

---

## 9. Persistence & JSON Schemas

Configuration and experiment results are saved as plain JSON files without requiring external database engines or third-party JSON libraries:

### Configuration JSON Schema (`config.json`)
```json
{
  "random_seed": 12345,
  "chunk_size": 1024,
  "heartbeat_interval": 100,
  "failure_timeout": 300,
  "source_client_id": 1,
  "destination_client_id": 2,
  "nodes": [
    { "id": 1, "name": "Client1", "type": "Client" },
    { "id": 3, "name": "Router3", "type": "Router" }
  ],
  "links": [
    { "node_a": 1, "node_b": 3, "bandwidth": 100, "latency": 5, "loss_probability": 0.0, "corruption_probability": 0.0, "queue_capacity": 10 }
  ]
}
```

### Experiment Results JSON Schema (`demonstration_recovery.json`)
```json
{
  "experiment_id": "EXP-1793151320714",
  "outcome": "SUCCESS",
  "total_ticks": 475,
  "configuration": { ... },
  "statistics": {
    "sent_count": 70,
    "delivered_count": 17,
    "dropped_count": 4,
    "queue_overflow_count": 0,
    "failure_drop_count": 4,
    "lost_count": 0,
    "corrupted_count": 0,
    "manual_failure_count": 1,
    "heartbeat_timeout_count": 0,
    "transfer_acknowledged_count": 4,
    "transfer_retransmitted_count": 8,
    "transfer_timeout_count": 8,
    "transfer_failed_count": 0
  },
  "route_changes": [],
  "packet_events": [ ... ]
}
```

---

## 10. Limitations & Risk Assessment

- **Model-Only Abstraction**: The simulator does not transmit real IP packets or open real TCP/UDP sockets. Real operating system network behavior (such as socket buffer exhaustion, OS-level packet drops, and MTU fragmentation) is not represented.
- **Single Active Transfer**: The simulator supports one active file transfer at a time. Concurrent multi-flow congestion control is documented as future work.
- **Educational Checksum (XOR)**: Packet data integrity uses a simple byte-wise XOR checksum. While suitable for classroom demonstrations of data corruption detection, XOR is non-cryptographic and does not protect against intentional tampering. CRC-32 is a recognized non-cryptographic industry alternative but is intentionally not implemented to keep data structures educational and accessible.
- **Link Directionality**: Links are modeled as undirected with symmetric properties in the primary demonstration. Asymmetric routing and directed link capacities can be explored in future extensions.
