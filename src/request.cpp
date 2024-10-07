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

#include "request.hpp"

#include "status_code.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>
#include <string>

namespace rg = std::ranges;
using std::format;
using std::from_chars;
using std::string;

auto
request::to_buffer() -> status_code::value {
  // ADS: use to_chars here
  const string s =
    format("{}\t{}\t{}\n", accession, methylome_size, n_intervals);
  rg::fill_n(begin(buf), buf_size, 0);
  rg::copy(s, begin(buf));
  return status_code::ok;
}

auto
request::from_buffer() -> status_code::value {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  accession.clear();
  methylome_size = 0;
  n_intervals = 0;

  const auto data_end = cbegin(buf) + buf_size;
  auto cursor = rg::find(cbegin(buf), data_end, delim);
  if (cursor == data_end) return status_code::malformed_accession;
  accession = string(cbegin(buf), rg::distance(cbegin(buf), cursor));
  if (*cursor++ != delim) return status_code::malformed_methylome_size;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, methylome_size);
    if (ec != std::errc{}) return status_code::malformed_methylome_size;
    cursor = ptr;
  }
  if (*cursor++ != delim) return status_code::malformed_n_intervals;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, n_intervals);
    if (ec != std::errc{}) return status_code::malformed_n_intervals;
    cursor = ptr;
  }
  if (*cursor++ != term) return status_code::malformed_n_intervals;
  return status_code::ok;
}

auto
request::summary() const -> string {
  return format("accession: {}\n"
                "methylome_size: {}\n"
                "n_intervals: {}",
                accession, methylome_size, n_intervals);
}

auto
request::summary_serial() const -> string {
  return format("{{\"accession\": \"{}\", "
                "\"methylome_size\": {}, "
                "\"n_intervals\": {}}}",
                accession, methylome_size, n_intervals);
}