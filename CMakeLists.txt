cmake_minimum_required(VERSION 3.20)
project(pbl2_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (EXISTS "C:/Qt/6.8.0/mingw_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.0/mingw_64")
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)

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
    src/services/RoutingService.cpp
    src/services/SimulationService.cpp
)

add_executable(pbl2_transfer_tests
    tests/test_transfer.cpp
    src/model/Packet.cpp
    src/services/TransferService.cpp
)

add_executable(pbl2_failure_recovery_tests
    tests/test_failure_recovery.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
    src/model/NetworkGraph.cpp
    src/services/RoutingService.cpp
    src/services/SimulationService.cpp
    src/services/TransferService.cpp
)

add_executable(pbl2_storage_tests
    tests/test_storage.cpp
    src/model/Node.cpp
    src/model/Client.cpp
    src/model/Router.cpp
    src/model/Packet.cpp
    src/model/Link.cpp
    src/model/NetworkGraph.cpp
    src/services/RoutingService.cpp
    src/services/SimulationService.cpp
    src/services/TransferService.cpp
    src/services/FileStorageService.cpp
)

target_include_directories(pbl2_tests PRIVATE . include)
target_include_directories(pbl2_model_tests PRIVATE . include)
target_include_directories(pbl2_graph_tests PRIVATE . include)
target_include_directories(pbl2_routing_tests PRIVATE . include)
target_include_directories(pbl2_simulation_tests PRIVATE . include)
target_include_directories(pbl2_transfer_tests PRIVATE . include)
target_include_directories(pbl2_failure_recovery_tests PRIVATE . include)
target_include_directories(pbl2_storage_tests PRIVATE . include)

if (MSVC)
    target_compile_options(pbl2_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_model_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_graph_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_routing_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_simulation_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_transfer_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_failure_recovery_tests PRIVATE /W4 /permissive-)
    target_compile_options(pbl2_storage_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(pbl2_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_model_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_graph_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_routing_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_simulation_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_transfer_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_failure_recovery_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pbl2_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME pbl2_data_structures COMMAND pbl2_tests)
add_test(NAME pbl2_model COMMAND pbl2_model_tests)
add_test(NAME pbl2_graph COMMAND pbl2_graph_tests)
add_test(NAME pbl2_routing COMMAND pbl2_routing_tests)
add_test(NAME pbl2_simulation COMMAND pbl2_simulation_tests)
add_test(NAME pbl2_transfer COMMAND pbl2_transfer_tests)
add_test(NAME pbl2_failure_recovery COMMAND pbl2_failure_recovery_tests)
add_test(NAME pbl2_storage COMMAND pbl2_storage_tests)

if (Qt6_FOUND)
    add_executable(pbl2_simulator
        src/main.cpp
        src/ui/MainWindow.cpp
        src/ui/TopologyCanvas.cpp
        include/ui/MainWindow.h
        include/ui/TopologyCanvas.h
        src/model/Node.cpp
        src/model/Client.cpp
        src/model/Router.cpp
        src/model/Packet.cpp
        src/model/Link.cpp
        src/model/NetworkGraph.cpp
        src/services/RoutingService.cpp
        src/services/SimulationService.cpp
        src/services/TransferService.cpp
        src/services/FileStorageService.cpp
    )
    target_include_directories(pbl2_simulator PRIVATE . include)
    target_link_libraries(pbl2_simulator PRIVATE Qt6::Widgets)

    add_executable(pbl2_ui_tests
        tests/test_ui_wiring.cpp
        src/ui/MainWindow.cpp
        src/ui/TopologyCanvas.cpp
        include/ui/MainWindow.h
        include/ui/TopologyCanvas.h
        src/model/Node.cpp
        src/model/Client.cpp
        src/model/Router.cpp
        src/model/Packet.cpp
        src/model/Link.cpp
        src/model/NetworkGraph.cpp
        src/services/RoutingService.cpp
        src/services/SimulationService.cpp
        src/services/TransferService.cpp
        src/services/FileStorageService.cpp
    )
    target_include_directories(pbl2_ui_tests PRIVATE . include)
    target_link_libraries(pbl2_ui_tests PRIVATE Qt6::Widgets)

    add_test(NAME pbl2_ui COMMAND pbl2_ui_tests)

    add_executable(pbl2_demonstration_tests
        tests/test_demonstration.cpp
        src/ui/MainWindow.cpp
        src/ui/TopologyCanvas.cpp
        include/ui/MainWindow.h
        include/ui/TopologyCanvas.h
        src/model/Node.cpp
        src/model/Client.cpp
        src/model/Router.cpp
        src/model/Packet.cpp
        src/model/Link.cpp
        src/model/NetworkGraph.cpp
        src/services/RoutingService.cpp
        src/services/SimulationService.cpp
        src/services/TransferService.cpp
        src/services/FileStorageService.cpp
    )
    target_include_directories(pbl2_demonstration_tests PRIVATE . include)
    target_link_libraries(pbl2_demonstration_tests PRIVATE Qt6::Widgets)

    add_test(NAME pbl2_demonstration COMMAND pbl2_demonstration_tests)
endif()

