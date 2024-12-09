# MIT License
#
# Copyright (c) 2024 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.30)

include_directories(${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest gtest_main)

add_executable(cpg_index_meta_test cpg_index_meta_test.cpp)
target_link_libraries(cpg_index_meta_test
 PRIVATE
 GTest::GTest
 Boost::json
 utilities
 cpg_index_meta
)

add_executable(cpg_index_test cpg_index_test.cpp)
target_link_libraries(cpg_index_test
 PRIVATE
 GTest::GTest
 Boost::json
 ZLIB::ZLIB
 Threads::Threads
 utilities
 cpg_index_meta
 cpg_index
)

add_executable(methylome_test methylome_test.cpp)
target_link_libraries(methylome_test
 PRIVATE
 GTest::GTest
 ZLIB::ZLIB
 methylome_metadata
 methylome
 utilities
)

add_executable(counts_file_formats_test counts_file_formats_test.cpp)
target_link_libraries(counts_file_formats_test
 PRIVATE
 GTest::GTest
 ZLIB::ZLIB
 counts_file_formats
 zlib_adapter
)

if(MXE_USE_STATIC_LIBS)
  add_library(static_target INTERFACE)
  target_link_options(static_target INTERFACE -static-libgcc -static-libstdc++)
  target_link_libraries(cpg_index_meta_test PRIVATE static_target)
  target_link_libraries(counts_file_formats_test PRIVATE static_target)
  target_link_libraries(cpg_index_test PRIVATE static_target)
  target_link_libraries(methylome_test PRIVATE static_target)
endif()

gtest_discover_tests(cpg_index_meta_test)
gtest_discover_tests(cpg_index_test)
gtest_discover_tests(methylome_test)
gtest_discover_tests(counts_file_formats_test)
