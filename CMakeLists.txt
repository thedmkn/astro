cmake_minimum_required(VERSION 4.3)
project(astro)

set(CMAKE_CXX_STANDARD 23 VERSION 1.0.0 LANGUAGES CXX)

add_library(astro INTERFACE)

add_library(astro::astro ALIAS astro)

target_include_directories(astro
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(astro INTERFACE cxx_std_20)