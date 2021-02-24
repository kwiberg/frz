# Developer documentation

Don’t forget to read the [technical details](technical-details.md)!

## Coding style

Frz follows the [Google C++ Style Guide][cppguide], with the following
changes:

  * We use exceptions.
  * Line-end comments should not be full sentences:

    `return;  // like this`

  * Indented statements always use braces:
  
    ```
    if (this) {
        That();
    }
    ```
    
  * Indent with 4 spaces.

[cppguide]: https://google.github.io/styleguide/cppguide.html

### C++ language version

Frz is written in C++20; however, since major compilers don’t yet
implement the full C++20 specification, we limit ourselves to the
C++20 subset supported by GCC 10.2.

This is likely to get somewhat less bleeding-edge as time passes.
Specifically, we’d like to support at least the latest stable version
of both GCC and clang.

### Code formatting

Except in unusual cases, rely on `clang-format` to format the source
code. You can format everything with this command:

`$ clang-format -i $(git ls-files '*.cc' '*.hh')`

Since everything should already be formatted this way, this is
supposed to bne a no-op for code with no local changes.

## Ninja

[Ninja](https://ninja-build.org/) is a good Make replacement. You can
ask CMake to generate Ninja files instead of Makefiles by giving it
the `-G Ninja` flag.

The rest of this document assumes Ninja, but you should be able to use
Make instead by just removing that flag from the CMake invocations.

## Different build configurations

Here’s how to generate different build configurations. (We start in
the source directory.)

### Release build

```
$ mkdir release ; cd release
$ cmake -G Ninja -D CMAKE_BUILD_TYPE=Release ..
```

### Debug build

```
$ mkdir debug ; cd debug
$ cmake -G Ninja -D CMAKE_BUILD_TYPE=Debug ..
```

### Address sanitizer build

```
$ mkdir asan ; cd asan
$ cmake -G Ninja -D CMAKE_BUILD_TYPE=Debug -D USE_SANITIZER=Address ..
```

## Running the tests

```
$ cd debug  # or whatever build directory you want
$ ctest --output-on-failure
```

## Running the benchmarks

The benchmarks test the speed of different hash functions.

```
$ cd release  # or whatever build directory you want
$ ./hasher_bench --benchmark_min_time=1
```

On Linux, you may want to run `sudo cpupower frequency-set --governor
performance` before running benchmarks. When you're done, you may want
to switch back from `performance` to `powersave`.

## Testing speed of hashing when reading a file from disk

1. Create an input file, if you don't already have something suitable:

   `$ head -c 1G /dev/urandom > my-little-file`
   
2. Force Linux to drop its disk cache. Otherwise, it'll read the file
   from RAM and not from the disk:
   
   `$ sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'`

3. Hash:

   `$ ./frz-hash-files /disk2/tmp/1G -a blake3 -m yes`

## Dependencies

The `frz` binary depends on these libraries:

| Library            | License                                     |
|--------------------|---------------------------------------------|
| [Abseil][abseil]   | [Apache-2.0][abseil-lic]                    |
| [Blake3][blake3]   | [Apache-2.0 or CC0-1.0][blake3-lic]         |
| [CLI11][cli11]     | [BSD-new][cli11-lic]                        |
| [libgit2][libgit2] | [GPL-2 with linking exception][libgit2-lic] |

[abseil]: https://github.com/abseil/abseil-cpp
[abseil-lic]: https://github.com/abseil/abseil-cpp/blob/master/LICENSE
[blake3]: https://github.com/BLAKE3-team/BLAKE3
[blake3-lic]: https://github.com/BLAKE3-team/BLAKE3/blob/master/LICENSE
[cli11]: https://github.com/CLIUtils/CLI11
[cli11-lic]: https://github.com/CLIUtils/CLI11/blob/master/LICENSE
[libgit2]: https://github.com/libgit2/libgit2
[libgit2-lic]: https://github.com/libgit2/libgit2/blob/main/COPYING

The unit tests and benchmarks additionally depend on these libraries:

| Library                   | License                                        |
|---------------------------|------------------------------------------------|
|[Google Benchmark][gbench] | [Apache-2.0][gbench-lic]                       |
|[Google Test][gtest]       | [BSD-new][gtest-lic]                           |
|[Nettle][nettle]           | [LGPL-3+ or GPL-2+][nettle-lic]                |
|[OpenSSL][openssl]         | [dual OpenSSL and SSLeay license][openssl-lic] |
                  
[gbench]: https://github.com/google/benchmark
[gbench-lic]: https://github.com/google/benchmark/blob/master/LICENSE
[gtest]: https://github.com/google/googletest
[gtest-lic]: https://github.com/google/googletest/blob/master/LICENSE
[nettle]: https://www.lysator.liu.se/~nisse/nettle/
[nettle-lic]: https://www.lysator.liu.se/~nisse/nettle/nettle.html#Copyright
[openssl]: https://www.openssl.org/
[openssl-lic]: https://www.openssl.org/source/license.html
