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

#ifndef SRC_CPG_INDEX_HPP_
#define SRC_CPG_INDEX_HPP_

#include "aligned_allocator.hpp"

#include <cstdint>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct genomic_interval;

struct cpg_index {
  typedef std::uint32_t cpg_pos_t;
  typedef std::vector<cpg_pos_t, aligned_allocator<cpg_pos_t>> vec;
  auto
  construct(const std::string &genome_file) -> int;
  auto
  read(const std::string &index_file) -> int;
  auto
  write(const std::string &index_file) const -> int;
  auto
  tostring() const -> std::string;

  // given the chromosome id (from chrom_index) and a position within
  // the chrom, get the offset of the CpG site from std::lower_bound
  auto
  get_offset_within_chrom(const std::int32_t ch_id,
                          const std::uint32_t pos) const -> std::uint32_t;
  auto
  get_offsets_within_chrom(
    const std::int32_t ch_id,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  // get the offset from the start of the data structure
  auto
  get_offsets(const std::int32_t ch_id,
              const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos)
    const -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  // get the offset from the start of the data structure given genomic
  // intervals
  auto
  get_offsets(const std::vector<genomic_interval> &gis) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  std::vector<std::string> chrom_order;
  std::vector<std::uint32_t> chrom_size;
  std::vector<cpg_index::vec> positions;
  std::vector<std::uint32_t> chrom_offset;
  std::unordered_map<std::string, std::int32_t> chrom_index;
  uint32_t n_cpgs_total{};
};

template <> struct std::formatter<cpg_index> : std::formatter<std::string> {
  auto
  format(const cpg_index &ci, std::format_context &ctx) const {
    return std::formatter<std::string>::format(ci.tostring(), ctx);
  }
};

#endif  // SRC_CPG_INDEX_HPP_