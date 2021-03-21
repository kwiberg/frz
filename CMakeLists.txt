cmake_minimum_required(VERSION 3.14)
project(frz)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)
include(FetchContent)
include(FindOpenSSL)
enable_testing()

if (USE_SANITIZER STREQUAL "Address")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
elseif (USE_SANITIZER STREQUAL "Thread")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
elseif (USE_SANITIZER STREQUAL "Undefined")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
elseif (USE_SANITIZER STREQUAL "Leak")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
elseif (USE_SANITIZER STREQUAL "Memory")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
endif()

# Pure C implementation of the Git core methods provided as a linkable library.
find_path(LIBGIT2_INCLUDE_DIR git2.h)
find_library(LIBGIT2_LIBRARY NAMES git2)
add_library(libgit2 SHARED IMPORTED)
target_include_directories(libgit2 SYSTEM INTERFACE ${LIBGIT2_INCLUDE_DIR})
set_property(TARGET libgit2 PROPERTY IMPORTED_LOCATION ${LIBGIT2_LIBRARY})

# Google's unit testing framework.
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(googletest)

# Google's benchmarking framework.
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        master
)
set(BENCHMARK_ENABLE_TESTING False)  # We don't want to run their unit tests.
FetchContent_MakeAvailable(benchmark)

# Abseil, Google's open-source collection of C++ library code designed
# to augment the C++ standard library.
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(abseil)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(CLI11)

# The Blake3 hash function. Doesn't come with a CMakeLists.txt, so we
# hack something up here.
FetchContent_Declare(
  blake3
  GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
  GIT_TAG        0.3.7
)
FetchContent_MakeAvailable(blake3)
enable_language(C ASM)
add_library(blake3 STATIC
  ${blake3_SOURCE_DIR}/c/blake3.c
  ${blake3_SOURCE_DIR}/c/blake3_dispatch.c
  ${blake3_SOURCE_DIR}/c/blake3_portable.c
  ${blake3_SOURCE_DIR}/c/blake3_sse2_x86-64_unix.S
  ${blake3_SOURCE_DIR}/c/blake3_sse41_x86-64_unix.S
  ${blake3_SOURCE_DIR}/c/blake3_avx2_x86-64_unix.S
  ${blake3_SOURCE_DIR}/c/blake3_avx512_x86-64_unix.S
  )
target_include_directories(blake3 INTERFACE ${blake3_SOURCE_DIR}/c)

macro(frz_target_compile_options name)
  target_compile_options(${name}
    PRIVATE -Werror -Wall -Wextra)
endmacro()

macro(frz_add_library name)
  add_library(${name} ${ARGN})
  frz_target_compile_options(${name})
endmacro()

macro(frz_add_executable name)
  add_executable(${name} ${ARGN})
  frz_target_compile_options(${name})
endmacro()

add_library(hash INTERFACE)
target_sources(hash INTERFACE src/hash.hh)
target_link_libraries(hash INTERFACE absl::int128)

add_library(hasher INTERFACE)
target_sources(hasher INTERFACE src/hasher.hh)
target_link_libraries(hasher INTERFACE hash stream)

add_library(exceptions INTERFACE)
target_sources(exceptions INTERFACE src/exceptions.hh)
target_link_libraries(exceptions INTERFACE absl::str_format)

add_library(filesystem_util STATIC src/filesystem_util.cc)

frz_add_library(log STATIC src/log.cc)
target_link_libraries(log
  PUBLIC
  absl::str_format
  absl::strings
  absl::time
  )

frz_add_executable(log_test src/log_test.cc)
target_link_libraries(log_test
  absl::time
  gmock
  gtest
  gtest_main
  log
  )

frz_add_library(blake3_256_hasher STATIC src/blake3_256_hasher.cc)
target_link_libraries(blake3_256_hasher
  PUBLIC hasher
  PRIVATE blake3)

frz_add_library(file_stream STATIC src/file_stream.cc)
target_link_libraries(file_stream PUBLIC exceptions stream)

frz_add_library(hash_index STATIC src/hash_index.cc)
target_link_libraries(hash_index
 PUBLIC
  hash
  log
 PRIVATE
  absl::flat_hash_map
  exceptions
  )

frz_add_library(stream STATIC src/stream.cc)
target_link_libraries(stream
 PUBLIC
  absl::base
 PRIVATE
  absl::synchronization
  worker
  )

frz_add_library(worker STATIC src/worker.cc)
target_link_libraries(worker
 PUBLIC
  absl::base
  absl::synchronization
  )

frz_add_library(content_store STATIC src/content_store.cc)
target_link_libraries(content_store
 PUBLIC
  stream
 PRIVATE
  absl::random_random
  exceptions
  file_stream
  filesystem_util
  )

frz_add_library(content_source STATIC src/content_source.cc)
target_link_libraries(content_source
 PUBLIC
  content_store
  hash
  hasher
  stream
 PRIVATE
  absl::flat_hash_map
  exceptions
  file_stream
  )

frz_add_library(frz_repository STATIC src/frz_repository.cc)
target_link_libraries(frz_repository
 PUBLIC
  stream
  hasher
 PRIVATE
  absl::flat_hash_set
  absl::node_hash_map
  content_source
  content_store
  exceptions
  file_stream
  hash_index
  log
  )

frz_add_library(openssl_blake2b512_hasher STATIC
  src/openssl_blake2b512_hasher.cc)
target_link_libraries(openssl_blake2b512_hasher
  PUBLIC hasher
  PRIVATE OpenSSL::Crypto)

frz_add_library(nettle_md5_hasher STATIC src/nettle_md5_hasher.cc)
target_link_libraries(nettle_md5_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(nettle_sha256_hasher STATIC src/nettle_sha256_hasher.cc)
target_link_libraries(nettle_sha256_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(nettle_sha512_hasher STATIC src/nettle_sha512_hasher.cc)
target_link_libraries(nettle_sha512_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(nettle_sha512_256_hasher STATIC src/nettle_sha512_256_hasher.cc)
target_link_libraries(nettle_sha512_256_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(nettle_sha3_256_hasher STATIC src/nettle_sha3_256_hasher.cc)
target_link_libraries(nettle_sha3_256_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(nettle_sha3_512_hasher STATIC src/nettle_sha3_512_hasher.cc)
target_link_libraries(nettle_sha3_512_hasher
  PUBLIC hasher PRIVATE -lnettle)

frz_add_library(openssl_md5_hasher STATIC src/openssl_md5_hasher.cc)
target_link_libraries(openssl_md5_hasher
  PUBLIC hasher
  PRIVATE OpenSSL::Crypto)

frz_add_library(openssl_sha256_hasher STATIC src/openssl_sha256_hasher.cc)
target_link_libraries(openssl_sha256_hasher
  PUBLIC hasher
  PRIVATE OpenSSL::Crypto)

frz_add_library(openssl_sha512_hasher STATIC src/openssl_sha512_hasher.cc)
target_link_libraries(openssl_sha512_hasher
  PUBLIC hasher
  PRIVATE OpenSSL::Crypto)

frz_add_library(openssl_sha512_256_hasher STATIC
  src/openssl_sha512_256_hasher.cc)
target_link_libraries(openssl_sha512_256_hasher
  PUBLIC hasher
  PRIVATE OpenSSL::Crypto)

frz_add_executable(hash_test src/hash_test.cc)
add_test(NAME hash COMMAND hash_test)
target_link_libraries(hash_test
  absl::random_random
  absl::str_format
  gmock
  gtest
  gtest_main
  hash
  )

frz_add_executable(content_store_test src/content_store_test.cc)
add_test(NAME content_store COMMAND content_store_test)
target_link_libraries(content_store_test
  content_store
  filesystem_testing
  gmock
  gtest
  gtest_main
  )

frz_add_executable(git_impl_test src/git_impl_test.cc)
add_test(NAME git_impl COMMAND git_impl_test)
target_link_libraries(git_impl_test
  filesystem_testing
  git_impl
  git_testing
  gmock
  gtest
  gtest_main
  )

frz_add_executable(git_test src/git_test.cc)
add_test(NAME git COMMAND git_test)
target_link_libraries(git_test
  filesystem_testing
  git
  git_testing
  gmock
  gtest
  gtest_main
  )

frz_add_executable(hasher_test src/hasher_test.cc)
add_test(NAME hasher COMMAND hasher_test)
target_link_libraries(hasher_test
  blake3_256_hasher
  gmock
  gtest
  gtest_main
  hasher
  nettle_md5_hasher
  nettle_sha256_hasher
  nettle_sha3_256_hasher
  nettle_sha3_512_hasher
  nettle_sha512_256_hasher
  nettle_sha512_hasher
  openssl_blake2b512_hasher
  openssl_md5_hasher
  openssl_sha256_hasher
  openssl_sha512_256_hasher
  openssl_sha512_hasher
  )

frz_add_executable(hasher_bench src/hasher_bench.cc)
target_link_libraries(hasher_bench
  benchmark
  blake3_256_hasher
  nettle_md5_hasher
  nettle_sha256_hasher
  nettle_sha3_256_hasher
  nettle_sha3_512_hasher
  nettle_sha512_256_hasher
  nettle_sha512_hasher
  openssl_blake2b512_hasher
  openssl_md5_hasher
  openssl_sha256_hasher
  openssl_sha512_256_hasher
  openssl_sha512_hasher
  )

frz_add_executable(frz-hash-files src/cmd_hash_files.cc)
target_link_libraries(frz-hash-files
  CLI11
  absl::str_format
  absl::time
  blake3_256_hasher
  file_stream
  hash_index
  openssl_sha256_hasher
  openssl_sha512_256_hasher
  stream
  )

frz_add_executable(frz-create-index src/cmd_create_index.cc)
target_link_libraries(frz-create-index
  CLI11
  absl::str_format
  blake3_256_hasher
  file_stream
  hash_index
  stream
  )

frz_add_library(command STATIC src/command.cc)
target_link_libraries(command
 PRIVATE
  CLI11
  absl::algorithm_container
  blake3_256_hasher
  exceptions
  frz_repository
  git
  log
  stream
  )

frz_add_library(filesystem_testing STATIC src/filesystem_testing.cc)
target_link_libraries(filesystem_testing
 PUBLIC
  gmock
  gtest
 PRIVATE
  absl::random_random
  )

add_library(git_impl STATIC src/git_impl.cc)
target_link_libraries(git_impl
 PUBLIC
  libgit2
  exceptions
 PRIVATE
  filesystem_util
  )

frz_add_library(git STATIC src/git.cc)
target_link_libraries(git
 PRIVATE
  libgit2
  absl::flat_hash_map
  git_impl
  )

frz_add_library(git_testing STATIC src/git_testing.cc)
target_link_libraries(git_testing
 PUBLIC
  gmock
  gtest
 PRIVATE
  libgit2
  absl::strings
  git_impl
  )

frz_add_executable(command_test src/command_test.cc)
add_test(NAME command COMMAND command_test)
target_link_libraries(command_test
 PRIVATE
  filesystem_testing
  gmock
  command
  gtest
  gtest_main
  )

frz_add_executable(command_add_test src/command_add_test.cc)
add_test(NAME command_add COMMAND command_add_test)
target_link_libraries(command_add_test
 PRIVATE
  absl::str_format
  command
  filesystem_testing
  git_testing
  gmock
  gtest
  gtest_main
  hash
  )

frz_add_executable(command_fill_test src/command_fill_test.cc)
add_test(NAME command_fill COMMAND command_fill_test)
target_link_libraries(command_fill_test
 PRIVATE
  filesystem_testing
  gmock
  command
  gtest
  gtest_main
  hash
  )

frz_add_executable(command_repair_test src/command_repair_test.cc)
add_test(NAME command_repair COMMAND command_repair_test)
target_link_libraries(command_repair_test
 PRIVATE
  filesystem_testing
  filesystem_util
  gmock
  command
  gtest
  gtest_main
  hash
  )

frz_add_executable(frz src/main.cc)
target_link_libraries(frz command)
