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

#include <methylome.hpp>

#include <methylome_metadata.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>

TEST(methylome_test, basic_assertions) {
  std::uint32_t n_meth{65536};
  std::uint32_t n_unmeth{65536};
  conditional_round_to_fit<methylome::m_count_t>(n_meth, n_unmeth);
  EXPECT_EQ(std::make_pair(n_meth, n_unmeth), std::make_pair(65535, 65535));
}

TEST(methylome_test, valid_read) {
  static constexpr auto filename{"data/SRX012345.m16"};
  const auto meta_filename = get_default_methylome_metadata_filename(filename);

  const auto [meta, meta_err] = methylome_metadata::read(meta_filename);
  EXPECT_FALSE(meta_err);

  const auto [meth, meth_err] = methylome::read(filename, meta);
  EXPECT_FALSE(meth_err);

  EXPECT_EQ(size(meth), 6053);
}
