cmake_minimum_required(VERSION 3.20)
project(pbl2_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_executable(pbl2_tests
    tests/test_data_structures.cpp
)

add_executable(pbl2_model_tests
    tests/test_model.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
)

add_executable(pbl2_graph_tests
    tests/test_graph.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
    src/model/NetworkGraph.cpp
)

add_executable(pbl2_routing_tests
    tests/test_routing.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
    src/model/NetworkGraph.cpp
    src/services/RoutingService.cpp
)

add_executable(pbl2_simulation_tests
    tests/test_simulation.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
    src/model/NetworkGraph.cpp
    src/services/SimulationService.cpp
)

target_include_directories(pbl2_tests PRIVATE . include)
target_include_directories(pbl2_model_tests PRIVATE . include)
target_include_directories(pbl2_graph_tests PRIVATE . include)
target_include_directories(pbl2_routing_tests PRIVATE . include)
target_include_directories(pbl2_simulation_tests PRIVATE . include)

if (MSVC)
    target_compile_options(pbl2_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_model_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_graph_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_routing_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_simulation_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(pbl2_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_model_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_graph_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_routing_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_simulation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME pbl2_data_structures COMMAND pbl2_tests)
add_test(NAME pbl2_model COMMAND pbl2_model_tests)
add_test(NAME pbl2_graph COMMAND pbl2_graph_tests)
add_test(NAME pbl2_routing COMMAND pbl2_routing_tests)
add_test(NAME pbl2_simulation COMMAND pbl2_simulation_tests)


