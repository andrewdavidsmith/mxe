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

# to find the version of cmake do
# $ cmake --version
cmake_minimum_required(VERSION 3.30)

add_library(logger OBJECT
  logger.hpp
  logger.cpp)

add_library(hash OBJECT
  hash.hpp
  hash.cpp)

add_library(utilities OBJECT
  utilities.hpp
  utilities.cpp)

add_library(zlib_adapter OBJECT
  zlib_adapter.hpp
  zlib_adapter.cpp)

add_library(counts_file_formats OBJECT
  counts_file_formats.hpp
  counts_file_formats.cpp)

add_library(genomic_interval OBJECT
  genomic_interval.hpp
  genomic_interval.cpp)

add_library(cpg_index OBJECT
  cpg_index.hpp
  cpg_index.cpp)

add_library(cpg_index_meta OBJECT
  cpg_index_meta.hpp
  cpg_index_meta.cpp)
target_include_directories(cpg_index_meta PRIVATE "${PROJECT_BINARY_DIR}")

add_library(cpg_index_set OBJECT
  cpg_index_set.hpp
  cpg_index_set.cpp)

add_library(methylome OBJECT
  methylome.hpp
  methylome.cpp)

add_library(methylome_metadata OBJECT
  methylome_metadata.hpp
  methylome_metadata.cpp)
target_include_directories(methylome_metadata PRIVATE "${PROJECT_BINARY_DIR}")

add_library(methylome_set OBJECT
  methylome_set.hpp
  methylome_set.cpp)

# sources for network
add_library(server OBJECT
  server.hpp
  server.cpp)

add_library(connection OBJECT
  connection.hpp
  connection.cpp)

add_library(response OBJECT
  response.hpp
  response.cpp)

add_library(request OBJECT
  request.hpp
  request.cpp)

add_library(request_handler OBJECT
  request_handler.hpp
  request_handler.cpp)

# sources with commands
add_library(command_compress OBJECT
  command_compress.hpp
  command_compress.cpp)

add_library(command_intervals OBJECT
  command_intervals.hpp
  command_intervals.cpp)

add_library(command_bins OBJECT
  command_bins.hpp
  command_bins.cpp)

add_library(command_format OBJECT
  command_format.hpp
  command_format.cpp)

add_library(command_check OBJECT
  command_check.hpp
  command_check.cpp)

add_library(command_merge OBJECT
  command_merge.hpp
  command_merge.cpp)

add_library(command_index OBJECT
  command_index.hpp
  command_index.cpp)

add_library(command_server OBJECT
  command_server.hpp
  command_server.cpp)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wfatal-errors)

add_executable(mxe)
target_include_directories(mxe PRIVATE "${PROJECT_BINARY_DIR}")
target_sources(mxe PRIVATE mxe.cpp)

if(MXE_USE_STATIC_LIBS)
  # ADS: this avoids the static linkage that aiso warns against
  target_link_options(mxe PRIVATE -static-libgcc -static-libstdc++)
endif()

target_link_libraries(
  mxe PRIVATE
  utilities
  logger
  genomic_interval
  cpg_index
  cpg_index_set
  cpg_index_meta
  methylome
  methylome_metadata
  methylome_set
  server
  connection
  request_handler
  request
  response
  zlib_adapter
  counts_file_formats
  command_index
  command_format
  command_check
  command_compress
  command_merge
  command_intervals
  command_server
  command_bins
  ZLIB::ZLIB
  Boost::boost
  Boost::program_options
  Boost::json
  Threads::Threads
)
install(TARGETS mxe DESTINATION bin)
