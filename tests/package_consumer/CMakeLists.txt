cmake_minimum_required(VERSION 3.25)
project(MicroWorkflowFrameworkPackageConsumer LANGUAGES CXX)

find_package(mwf 0.1 REQUIRED CONFIG)
add_executable(mwf_package_consumer main.cpp)
target_link_libraries(mwf_package_consumer PRIVATE vosp::workflow)
target_compile_features(mwf_package_consumer PRIVATE cxx_std_23)
