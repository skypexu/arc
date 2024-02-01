cmake_minimum_required(VERSION 3.22)
project(arccache LANGUAGES CXX VERSION 1.0)

include(GNUInstallDirs)

option(ENABLE_TEST "enable unit test" true)

add_library(fengge_arc INTERFACE)
add_library(Fengge::fengge_arc ALIAS fengge_arc)
target_include_directories(fengge_arc
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fengge>
)

if (ENABLE_TEST)
include(GoogleTest)
enable_testing()

set(FETCHCONTENT_QUIET OFF)
include(FetchContent)
FetchContent_Declare(googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)

FetchContent_GetProperties(googletest)
if (NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

add_executable(arc_test tests/arc_test.cpp)
target_link_libraries(arc_test Fengge::fengge_arc gtest_main gtest)

gtest_add_tests(TARGET arc_test)
endif(ENABLE_TEST)

install(TARGETS fengge_arc
        EXPORT FenggeARC
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
            src/include/fengge/arc.h
            src/include/fengge/cache_traits.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fengge)
install(EXPORT FenggeARC
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FenggeARC
        FILE FenggeARCConfig.cmake
        NAMESPACE Fengge::)

write_basic_package_version_file(version_check
VERSION 1.2
COMPATIBILITY AnyNewerVersion
)
