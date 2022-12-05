# Based on the officail Boost Filesystem port, modified to include a patch from 1.77

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO boostorg/filesystem
    REF boost-1.75.0
    SHA512 e79008f39568eaa4763110e4c172b022220b0667d7f05c606daed92f6f5c3977f2478063b6b16db6517b3e33b2df8ec13f8f0ae134fb2020a93d64895170b08d
    HEAD_REF master
    PATCHES
        0001-Added-config-macros-for-disabling-use-of-some-system.patch
)

if(NOT DEFINED CURRENT_HOST_INSTALLED_DIR)
    message(FATAL_ERROR "boost-filesystem requires a newer version of vcpkg in order to build.")
endif()

include(${CURRENT_HOST_INSTALLED_DIR}/share/boost-build/boost-modular-build.cmake)
boost_modular_build(
    SOURCE_PATH ${SOURCE_PATH}
    BOOST_CMAKE_FRAGMENT ${CURRENT_PORT_DIR}/config.cmake
    )
include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})