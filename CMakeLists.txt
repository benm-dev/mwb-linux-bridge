cmake_minimum_required(VERSION 3.10)
project(mwb_client CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL for AES decryption/encryption matching PowerToys MWB
find_package(OpenSSL REQUIRED)
# Find libevdev for seamless Wayland/X11 input injection via Input Leap strategy
find_package(PkgConfig REQUIRED)
pkg_check_modules(EVDEV REQUIRED libevdev)

add_executable(mwb_client
    src/main.cpp
    src/CryptoHelper.cpp
    src/InputManager.cpp
    src/NetworkManager.cpp
)

target_include_directories(mwb_client PRIVATE src)
target_include_directories(mwb_client PRIVATE ${OPENSSL_INCLUDE_DIR})
target_include_directories(mwb_client PRIVATE ${EVDEV_INCLUDE_DIRS})

target_link_libraries(mwb_client PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    ${EVDEV_LIBRARIES}
    pthread
)
