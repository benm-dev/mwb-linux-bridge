cmake_minimum_required(VERSION 3.10)
project(mwb_linux_bridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find OpenSSL for AES decryption/encryption matching PowerToys MWB
find_package(OpenSSL REQUIRED)

add_executable(mwb_client
    src/main.cpp
    src/CryptoHelper.cpp
    src/InputManager.cpp
    src/NetworkManager.cpp
)

target_include_directories(mwb_client PRIVATE src)
target_include_directories(mwb_client PRIVATE ${OPENSSL_INCLUDE_DIR})

target_link_libraries(mwb_client PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    pthread
)

target_compile_options(mwb_client PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wformat=2
    -Wshadow
    -Wno-missing-field-initializers
)

install(TARGETS mwb_client RUNTIME DESTINATION bin RENAME mwb-client)
install(PROGRAMS tui/mwb-tui DESTINATION bin)
