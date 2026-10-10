cmake_minimum_required(VERSION 3.24)
project(VisionAIPlatformWorkspace VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VISIONAI_BUILD_TESTS "构建 VisionAI C++ 测试" ON)
option(VISIONAI_ENABLE_ASAN "启用 AddressSanitizer（地址消毒器）检查越界和释放后使用" OFF)
option(VISIONAI_ENABLE_STATIC_ANALYSIS "启用编译器静态分析（MSVC /analyze）" OFF)
option(VISIONAI_WARNINGS_AS_ERRORS "将编译器警告视为错误，适用于严格 CI（持续集成）" OFF)

find_package(Threads REQUIRED)

function(visionai_apply_warnings target_name)
    if(MSVC)
        # /sdl：启用额外安全开发生命周期检查；/utf-8：保证中文注释和字符串跨机器一致。
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8 /sdl)

        if(VISIONAI_ENABLE_STATIC_ANALYSIS)
            target_compile_options(${target_name} PRIVATE /analyze)
        endif()

        if(VISIONAI_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()

        if(VISIONAI_ENABLE_ASAN)
            target_compile_options(${target_name} PRIVATE /fsanitize=address)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)

        if(VISIONAI_ENABLE_ASAN)
            target_compile_options(
                ${target_name}
                PRIVATE
                    -fsanitize=address,undefined
                    -fno-omit-frame-pointer
            )
        endif()

        if(VISIONAI_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# L0 Contracts（稳定契约层）
add_library(visionai_contracts INTERFACE)
target_include_directories(
    visionai_contracts
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Contracts
)
add_library(VisionAI::Contracts ALIAS visionai_contracts)

# L1 SDK / Ports（稳定接口层）
add_library(visionai_runtime_sdk INTERFACE)
target_include_directories(
    visionai_runtime_sdk
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Foundation/SDK
)
target_link_libraries(visionai_runtime_sdk INTERFACE visionai_contracts)
add_library(VisionAI::RuntimeSDK ALIAS visionai_runtime_sdk)

add_library(visionai_algorithm_sdk INTERFACE)
target_include_directories(
    visionai_algorithm_sdk
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Foundation/SDK
)
target_link_libraries(visionai_algorithm_sdk INTERFACE visionai_contracts)
add_library(VisionAI::AlgorithmSDK ALIAS visionai_algorithm_sdk)

# L2 Edge Runtime Kernel（边缘实时运行内核）
add_library(visionai_edge_core STATIC
    EdgeRuntime/Core/ThreadPool.cpp
)
target_include_directories(visionai_edge_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_link_libraries(visionai_edge_core PUBLIC Threads::Threads)
target_compile_features(visionai_edge_core PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_core)
add_library(VisionAI::EdgeCore ALIAS visionai_edge_core)

add_library(visionai_edge_observability STATIC
    EdgeRuntime/Observability/HealthRegistry.cpp
)
target_include_directories(visionai_edge_observability PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_compile_features(visionai_edge_observability PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_observability)
add_library(VisionAI::EdgeObservability ALIAS visionai_edge_observability)

add_library(visionai_edge_host STATIC
    EdgeRuntime/Host/RuntimeHost.cpp
)
target_include_directories(visionai_edge_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_link_libraries(
    visionai_edge_host
    PUBLIC
        visionai_edge_core
        visionai_edge_observability
)
target_compile_features(visionai_edge_host PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_host)
add_library(VisionAI::EdgeHost ALIAS visionai_edge_host)

add_library(visionai_edge_memory STATIC
    EdgeRuntime/Memory/BufferPool.cpp
)
target_include_directories(visionai_edge_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_compile_features(visionai_edge_memory PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_memory)
add_library(VisionAI::EdgeMemory ALIAS visionai_edge_memory)

add_library(visionai_edge_frame STATIC
    EdgeRuntime/Frame/FrameBroker.cpp
)
target_include_directories(visionai_edge_frame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_link_libraries(visionai_edge_frame PUBLIC visionai_contracts)
target_compile_features(visionai_edge_frame PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_frame)
add_library(VisionAI::EdgeFrame ALIAS visionai_edge_frame)

add_library(visionai_edge_graph STATIC
    EdgeRuntime/Graph/GraphExecutor.cpp
)
target_include_directories(visionai_edge_graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_compile_features(visionai_edge_graph PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_graph)
add_library(VisionAI::EdgeGraph ALIAS visionai_edge_graph)

add_library(visionai_edge_scheduler STATIC
    EdgeRuntime/Scheduler/ExecutionScheduler.cpp
)
target_include_directories(visionai_edge_scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_compile_features(visionai_edge_scheduler PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_edge_scheduler)
add_library(VisionAI::EdgeScheduler ALIAS visionai_edge_scheduler)

add_library(visionai_edge_kernel INTERFACE)
target_link_libraries(
    visionai_edge_kernel
    INTERFACE
        visionai_edge_core
        visionai_edge_observability
        visionai_edge_host
        visionai_edge_memory
        visionai_edge_frame
        visionai_edge_graph
        visionai_edge_scheduler
)
add_library(VisionAI::EdgeKernel ALIAS visionai_edge_kernel)

# L3 Infrastructure Adapters（基础设施适配层）
add_library(visionai_runtime_onnx STATIC
    RuntimeAdapters/ONNXRuntime/OnnxRuntimeAdapter.cpp
)
target_include_directories(
    visionai_runtime_onnx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeAdapters
)
target_link_libraries(visionai_runtime_onnx PUBLIC visionai_runtime_sdk)
target_compile_features(visionai_runtime_onnx PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_runtime_onnx)
add_library(VisionAI::RuntimeONNX ALIAS visionai_runtime_onnx)

# L4 AI Capabilities（AI 原子能力层）
add_library(visionai_algorithm_tracking STATIC
    Packages/Algorithms/Tracking/SimpleIoUTracker.cpp
)
target_include_directories(
    visionai_algorithm_tracking
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Algorithms
)
target_link_libraries(visionai_algorithm_tracking PUBLIC visionai_contracts visionai_algorithm_sdk)
target_compile_features(visionai_algorithm_tracking PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_algorithm_tracking)
add_library(VisionAI::AlgorithmTracking ALIAS visionai_algorithm_tracking)

add_library(visionai_algorithm_spatial STATIC
    Packages/Algorithms/Spatial/ZoneMembership.cpp
)
target_include_directories(
    visionai_algorithm_spatial
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Algorithms
)
target_link_libraries(visionai_algorithm_spatial PUBLIC visionai_contracts visionai_algorithm_sdk)
target_compile_features(visionai_algorithm_spatial PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_algorithm_spatial)
add_library(VisionAI::AlgorithmSpatial ALIAS visionai_algorithm_spatial)

add_library(visionai_algorithm_temporal STATIC
    Packages/Algorithms/Temporal/DurationTracker.cpp
)
target_include_directories(
    visionai_algorithm_temporal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Algorithms
)
target_link_libraries(visionai_algorithm_temporal PUBLIC visionai_algorithm_sdk)
target_compile_features(visionai_algorithm_temporal PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_algorithm_temporal)
add_library(VisionAI::AlgorithmTemporal ALIAS visionai_algorithm_temporal)

add_library(visionai_platform_algorithms INTERFACE)
target_link_libraries(
    visionai_platform_algorithms
    INTERFACE
        visionai_algorithm_tracking
        visionai_algorithm_spatial
        visionai_algorithm_temporal
)
add_library(VisionAI::PlatformAlgorithms ALIAS visionai_platform_algorithms)

# L5 Orchestration（编排与智能决策层）
add_library(visionai_rule_engine STATIC
    EdgeRuntime/RuleEngineLite/RuleEngine.cpp
)
target_include_directories(visionai_rule_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_compile_features(visionai_rule_engine PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_rule_engine)
add_library(VisionAI::RuleEngine ALIAS visionai_rule_engine)

add_library(visionai_event_engine STATIC
    EdgeRuntime/EventEngineLite/EventEngine.cpp
)
target_include_directories(visionai_event_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EdgeRuntime)
target_link_libraries(visionai_event_engine PUBLIC visionai_contracts)
target_compile_features(visionai_event_engine PUBLIC cxx_std_20)
visionai_apply_warnings(visionai_event_engine)
add_library(VisionAI::EventEngine ALIAS visionai_event_engine)

add_library(visionai_edge_intelligence INTERFACE)
target_link_libraries(
    visionai_edge_intelligence
    INTERFACE
        visionai_rule_engine
        visionai_event_engine
)
add_library(VisionAI::EdgeIntelligence ALIAS visionai_edge_intelligence)

# Tests（系统级自动化测试）
if(VISIONAI_BUILD_TESTS)
    enable_testing()

    add_executable(visionai_core_tests Tests/core_tests.cpp)
    target_link_libraries(
        visionai_core_tests
        PRIVATE
            visionai_contracts
            visionai_edge_kernel
            visionai_edge_intelligence
            visionai_platform_algorithms
    )
    visionai_apply_warnings(visionai_core_tests)

    add_executable(
        visionai_runtime_primitives_tests
        Tests/runtime_primitives_tests.cpp
    )
    target_link_libraries(
        visionai_runtime_primitives_tests
        PRIVATE
            visionai_edge_core
            visionai_edge_observability
    )
    visionai_apply_warnings(visionai_runtime_primitives_tests)

    add_executable(
        visionai_runtime_host_tests
        Tests/runtime_host_tests.cpp
    )
    target_link_libraries(
        visionai_runtime_host_tests
        PRIVATE
            visionai_edge_host
    )
    visionai_apply_warnings(visionai_runtime_host_tests)

    # GCC/Clang（GNU/LLVM 编译器）的 Sanitizer（运行时消毒器）还需要在最终可执行文件链接阶段启用。
    if(VISIONAI_ENABLE_ASAN AND NOT MSVC)
        target_link_options(
            visionai_core_tests
            PRIVATE
                -fsanitize=address,undefined
        )
        target_link_options(
            visionai_runtime_primitives_tests
            PRIVATE
                -fsanitize=address,undefined
        )
        target_link_options(
            visionai_runtime_host_tests
            PRIVATE
                -fsanitize=address,undefined
        )
    endif()

    add_test(NAME visionai_core_tests COMMAND visionai_core_tests)
    add_test(
        NAME visionai_runtime_primitives_tests
        COMMAND visionai_runtime_primitives_tests
    )
    add_test(
        NAME visionai_runtime_host_tests
        COMMAND visionai_runtime_host_tests
    )

    # 防止单测死锁让 CI 永久挂起；超时后由日志和独立模块测试定位问题。
    set_tests_properties(
        visionai_core_tests
        visionai_runtime_primitives_tests
        visionai_runtime_host_tests
        PROPERTIES TIMEOUT 30
    )
endif()
