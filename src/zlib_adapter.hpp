/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SRC_ZLIB_ADAPTER_HPP_
#define SRC_ZLIB_ADAPTER_HPP_

#include <zlib.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// clang-format off
// ADS: from zlib/zlib.h
// #define Z_OK            0
// #define Z_STREAM_END    1
// #define Z_NEED_DICT     2
// #define Z_ERRNO        (-1)
// #define Z_STREAM_ERROR (-2)
// #define Z_DATA_ERROR   (-3)
// #define Z_MEM_ERROR    (-4)
// #define Z_BUF_ERROR    (-5)
// #define Z_VERSION_ERROR (-6)
// clang-format on
enum class zlib_adapter_error : std::uint32_t {
  ok = 0,
  z_stream_end = 1,
  z_need_dict = 2,
  z_errno = 3,
  z_stream_error = 4,
  z_data_error = 5,
  z_mem_error = 6,
  z_buf_error = 7,
  z_version_error = 8,
  unexpected_return_code = 9,
};

// register zlib_adapter_error as error code enum
template <>
struct std::is_error_code_enum<zlib_adapter_error> : public std::true_type {};

// category to provide text descriptions
struct zlib_adapter_error_category : std::error_category {
  const char *name() const noexcept override { return "zlib_adapter_error"; }
  std::string message(int code) const override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "Z_STREAM_END"s;
    case 2: return "Z_NEED_DICT"s;
    case 3: return "Z_ERRNO"s;
    case 4: return "Z_STREAM_ERROR"s;
    case 5: return "Z_DATA_ERROR"s;
    case 6: return "Z_MEM_ERROR"s;
    case 7: return "Z_BUF_ERROR"s;
    case 8: return "Z_VERSION_ERROR"s;
    case 9: return "unexpected return code from zlib"s;
    }
    // clang-format on
    std::unreachable();  // hopefully this is unreacheable
  }
};

inline std::error_code
make_error_code(zlib_adapter_error e) {
  static auto category = zlib_adapter_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

template <typename T>
[[nodiscard]] auto
compress(const T &in, std::vector<std::uint8_t> &out) -> std::error_code {
  z_stream strm{};
  {
    const int ret = deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS,
                                 MAX_MEM_LEVEL, Z_RLE);
    switch (ret) {
    case Z_VERSION_ERROR:
      return zlib_adapter_error::z_version_error;
    case Z_STREAM_ERROR:
      return zlib_adapter_error::z_stream_error;
    }
    assert(ret == Z_OK);
  }

  // pointer to bytes to compress
  // ADS: 'next_in' is 'z_const' defined as 'const' in zconf.h
  strm.next_in = reinterpret_cast<std::uint8_t *>(
    const_cast<typename T::value_type *>(in.data()));
  // bytes available to compress
  const auto n_input_bytes = size(in) * sizeof(typename T::value_type);
  strm.avail_in = n_input_bytes;

  out.resize(deflateBound(&strm, n_input_bytes));

  strm.next_out = out.data();  // pointer to bytes for compressed data
  strm.avail_out = size(out);  // bytes available for compressed data

  {
    // ADS: here we want (ret == Z_STREAM_END) and not (ret == Z_OK)
    const int ret = deflate(&strm, Z_FINISH);
    // clang-format off
    switch (ret) {
    case Z_STREAM_ERROR: return zlib_adapter_error::z_stream_error;
    case Z_BUF_ERROR: return zlib_adapter_error::z_buf_error;
    }
    // clang-format on
    assert(ret == Z_STREAM_END);
  }

  {
    const int ret = deflateEnd(&strm);
    // clang-format off
    switch (ret) {
    case Z_STREAM_ERROR: return zlib_adapter_error::z_stream_error;
    case Z_DATA_ERROR: return zlib_adapter_error::z_data_error;
    }
    // clang-format on
    assert(ret == Z_OK);  // what about STREAM_END?
    // ADS: if deflateEnd was ok, do we want (ret == Z_STREAM_END)???
  }

  out.resize(strm.total_out);

  return zlib_adapter_error::ok;
}

template <typename T>
[[nodiscard]] static inline auto
decompress(std::vector<std::uint8_t> &in, T &out) -> std::error_code {
  z_stream strm{};
  {
    const int ret = inflateInit(&strm);
    // clang-format off
    switch (ret) {
    case Z_VERSION_ERROR: return zlib_adapter_error::z_version_error;
    case Z_STREAM_ERROR: return zlib_adapter_error::z_stream_error;
    case Z_MEM_ERROR: return zlib_adapter_error::z_mem_error;
    }
    // clang-format on
    assert(ret == Z_OK);
  }

  strm.next_in = in.data();  // pointer to compressed bytes
  strm.avail_in = size(in);  // bytes available to decompress

  // pointer to bytes for decompressed data
  strm.next_out = reinterpret_cast<std::uint8_t *>(out.data());
  // bytes available for decompressed data
  const auto n_output_bytes = size(out) * sizeof(typename T::value_type);
  strm.avail_out = n_output_bytes;

  {
    const int ret = inflate(&strm, Z_FINISH);
    if (ret == Z_NEED_DICT || ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
      inflateEnd(&strm);
    // clang-format off
    switch (ret) {
    case Z_STREAM_ERROR: return zlib_adapter_error::z_stream_error;
    case Z_NEED_DICT: return zlib_adapter_error::z_need_dict;
    case Z_MEM_ERROR: return zlib_adapter_error::z_mem_error;
    case Z_DATA_ERROR: return zlib_adapter_error::z_data_error;
    case Z_BUF_ERROR: return zlib_adapter_error::z_buf_error;
    }
    // clang-format on
    assert(ret == Z_STREAM_END);
    // ADS: above, what about Z_OK??
  }

  if (inflateEnd(&strm) == Z_STREAM_ERROR)
    return zlib_adapter_error::z_stream_error;

  // assume size(out) is an exact fit
  // out.resize(strm.total_out);
  // ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;

  return zlib_adapter_error::ok;
}

#endif  // SRC_ZLIB_ADAPTER_HPP_