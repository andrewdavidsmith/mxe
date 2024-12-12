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

#ifndef SRC_CONFIG_FILE_UTILS_HPP_
#define SRC_CONFIG_FILE_UTILS_HPP_

#include <boost/describe.hpp>
#include <boost/mp11.hpp>

#include <format>
#include <fstream>
#include <print>
#include <ranges>
#include <string>
#include <system_error>

[[nodiscard]] inline auto
format_as_config(const auto &t) -> std::string {
  using T = std::remove_cvref_t<decltype(t)>;
  using members =
    boost::describe::describe_members<T, boost::describe::mod_any_access>;
  std::string r;
  boost::mp11::mp_for_each<members>([&](const auto &member) {
    std::string name(member.name);
    std::ranges::replace(name, '_', '-');
    r += std::format("{} = {}\n", name, t.*member.pointer);
  });
  return r;
}

[[nodiscard]] inline auto
write_config_file(const auto &args, [[maybe_unused]] const std::string &header =
                                      "") -> std::error_code {
  [[maybe_unused]] static constexpr auto header_width = 78;
  std::ofstream out(args.config_out);
  if (!out) {
    const auto ec = std::make_error_code(std::errc(errno));
    std::println("Failed to open config file {}: {}", args.config_out, ec);
    return ec;
  }

  std::print(out, "{}", format_as_config(args));
  if (!out) {
    const auto ec = std::make_error_code(std::errc(errno));
    std::println("Failed to write config file {}: {}", args.config_file, ec);
    return ec;
  }
  return {};
}

#endif  // SRC_CONFIG_FILE_UTILS_HPP_