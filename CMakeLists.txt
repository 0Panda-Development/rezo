cmake_minimum_required(VERSION 3.21)
project(Rezo LANGUAGES CXX)

# CEF 151 requires C++20 (adapted from brief's C++17).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef" CACHE PATH "CEF root")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
# CEF 151 removed cef_add_target/add_subdirectory(CEF_ROOT); use find_package(CEF).
# Sandbox off: brief's main.cpp uses settings.no_sandbox = true (plain exe, not
# the bootstrap.exe/DLL scheme that USE_SANDBOX=ON builds).
set(USE_SANDBOX OFF CACHE BOOL "Enable or disable use of the sandbox.")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

SET_CEF_TARGET_OUT_DIR()
ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

add_executable(rezo src/main.cpp)
SET_EXECUTABLE_TARGET_PROPERTIES(rezo)
add_dependencies(rezo libcef_dll_wrapper)
target_link_libraries(rezo libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
COPY_FILES("rezo" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("rezo" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

enable_testing()
add_executable(rezo_tests tests/test_main.cpp)
add_test(NAME rezo_tests COMMAND rezo_tests)