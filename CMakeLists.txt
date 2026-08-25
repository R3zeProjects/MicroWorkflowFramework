cmake_minimum_required(VERSION 3.25)

project(MicroWorkflowFramework VERSION 0.1.0 LANGUAGES CXX)
set(MICRO_WORKFLOW_FRAMEWORK_PRERELEASE "beta")

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

option(MWF_BUILD_EXAMPLES "Build MWF examples" ON)
option(MWF_BUILD_BENCHMARKS "Build opt-in MWF benchmarks" OFF)
option(MWF_BUILD_MEF_INTEGRATION "Build direct integration with pinned MEF" OFF)
option(MWF_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MWF_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)
option(MWF_FETCH_CONTRACTS "Fetch pinned MCF contracts when not installed" ON)
set(MWF_CONTRACTS_SOURCE_DIR "" CACHE PATH "Optional MicroContractsFramework source tree")

if(MWF_ENABLE_SANITIZERS AND MWF_ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR "ASan/UBSan and ThreadSanitizer require separate builds")
endif()

if(NOT TARGET vosp::contracts)
    if(MWF_CONTRACTS_SOURCE_DIR)
        if(NOT EXISTS "${MWF_CONTRACTS_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "MWF_CONTRACTS_SOURCE_DIR does not contain MCF")
        endif()
        set(_mwf_saved_build_testing "${BUILD_TESTING}")
        set(BUILD_TESTING OFF)
        add_subdirectory("${MWF_CONTRACTS_SOURCE_DIR}"
                         "${CMAKE_CURRENT_BINARY_DIR}/contracts" EXCLUDE_FROM_ALL)
        set(BUILD_TESTING "${_mwf_saved_build_testing}")
        unset(_mwf_saved_build_testing)
    else()
        find_package(vosp_contracts 0.6 QUIET CONFIG)
        if(NOT vosp_contracts_FOUND)
            if(NOT MWF_FETCH_CONTRACTS)
                message(FATAL_ERROR
                    "MicroContractsFramework 0.6 is required. Install it, set "
                    "MWF_CONTRACTS_SOURCE_DIR, or enable MWF_FETCH_CONTRACTS.")
            endif()
            include(FetchContent)
            FetchContent_Declare(vosp_contracts
                GIT_REPOSITORY https://github.com/R3zeProjects/MicroContractsFramework.git
                GIT_TAG e69892633f0d863424a5b76c9730c2456691e564
                GIT_SHALLOW FALSE)
            set(_mwf_saved_build_testing "${BUILD_TESTING}")
            set(BUILD_TESTING OFF)
            FetchContent_MakeAvailable(vosp_contracts)
            set(BUILD_TESTING "${_mwf_saved_build_testing}")
            unset(_mwf_saved_build_testing)
        endif()
    endif()
endif()

if(WIN32)
    set(_mwf_platform_source src/process_windows.cpp)
elseif(UNIX)
    set(_mwf_platform_source src/process_posix.cpp)
else()
    message(FATAL_ERROR "MicroWorkflowFramework supports Windows and POSIX platforms")
endif()

add_library(MicroWorkflowFramework STATIC ${_mwf_platform_source})
add_library(vosp::workflow ALIAS MicroWorkflowFramework)
set_target_properties(MicroWorkflowFramework PROPERTIES
    EXPORT_NAME workflow
    CXX_EXTENSIONS OFF)
target_compile_features(MicroWorkflowFramework PUBLIC cxx_std_23)
target_include_directories(MicroWorkflowFramework PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(MicroWorkflowFramework PUBLIC vosp::contracts Threads::Threads)

if(MSVC)
    target_compile_options(MicroWorkflowFramework PRIVATE /W4 /permissive-)
else()
    target_compile_options(MicroWorkflowFramework PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(mwf_enable_sanitizers target)
    if(MWF_ENABLE_SANITIZERS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    elseif(MWF_ENABLE_THREAD_SANITIZER)
        if(MSVC)
            message(FATAL_ERROR "ThreadSanitizer is unavailable with MSVC")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    endif()
endfunction()

if(BUILD_TESTING)
    add_library(MicroWorkflowFrameworkHeaderContracts OBJECT
        tests/header_contracts/runner_header.cpp
        tests/header_contracts/types_header.cpp
        tests/header_contracts/version_header.cpp
        tests/header_contracts/workflow_header.cpp)
    target_link_libraries(MicroWorkflowFrameworkHeaderContracts
        PRIVATE MicroWorkflowFramework)

    add_executable(MicroWorkflowFrameworkTests tests/workflow_tests.cpp)
    target_link_libraries(MicroWorkflowFrameworkTests PRIVATE MicroWorkflowFramework)
    mwf_enable_sanitizers(MicroWorkflowFrameworkTests)
    add_test(NAME MicroWorkflowFramework.Tests COMMAND MicroWorkflowFrameworkTests)

    add_executable(MicroWorkflowFrameworkInvalidModel EXCLUDE_FROM_ALL
        tests/compile_fail/invalid_model.cpp)
    target_link_libraries(MicroWorkflowFrameworkInvalidModel
        PRIVATE MicroWorkflowFramework)
    add_test(NAME MicroWorkflowFramework.CompileFail.InvalidModel
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --config $<CONFIG> --target MicroWorkflowFrameworkInvalidModel)
    set_tests_properties(MicroWorkflowFramework.CompileFail.InvalidModel
        PROPERTIES WILL_FAIL TRUE)
endif()

if(MWF_BUILD_EXAMPLES)
    add_executable(MicroWorkflowFrameworkExample examples/basic.cpp)
    target_link_libraries(MicroWorkflowFrameworkExample PRIVATE MicroWorkflowFramework)
    if(BUILD_TESTING)
        add_test(NAME MicroWorkflowFramework.Example.Basic
            COMMAND MicroWorkflowFrameworkExample)
    endif()
endif()

if(MWF_BUILD_BENCHMARKS)
    add_executable(MicroWorkflowFrameworkBenchmark benchmarks/mwf_benchmark.cpp)
    target_link_libraries(MicroWorkflowFrameworkBenchmark PRIVATE MicroWorkflowFramework)
endif()

if(MWF_BUILD_MEF_INTEGRATION)
    include(FetchContent)
    set(MEF_CONTRACTS_SOURCE_DIR "${MWF_CONTRACTS_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(MEF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL_COMPARISON_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(mwf_mef
        GIT_REPOSITORY https://github.com/R3zeProjects/MicroErrorFramework.git
        GIT_TAG 8c718fc49c65c82d7451434194b024b8e0197dec
        GIT_SHALLOW FALSE)
    set(_mwf_saved_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    FetchContent_MakeAvailable(mwf_mef)
    set(BUILD_TESTING "${_mwf_saved_build_testing}")
    unset(_mwf_saved_build_testing)

    add_executable(MicroWorkflowFrameworkMefIntegration tests/mef_integration.cpp)
    target_link_libraries(MicroWorkflowFrameworkMefIntegration
        PRIVATE MicroWorkflowFramework vosp::vosp)
    add_test(NAME MicroWorkflowFramework.MefIntegration
        COMMAND MicroWorkflowFrameworkMefIntegration)
endif()

install(TARGETS MicroWorkflowFramework EXPORT mwfTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION
    ${CMAKE_INSTALL_DATADIR}/licenses/MicroWorkflowFramework)
install(EXPORT mwfTargets FILE mwfTargets.cmake NAMESPACE vosp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwf)
configure_package_config_file(cmake/mwfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mwfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwf)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mwfConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mwfConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mwfConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwf)
