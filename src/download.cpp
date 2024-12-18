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

#include "download.hpp"

// Can't silence IWYU on these
#include <boost/core/detail/string_view.hpp>
#include <boost/intrusive/detail/list_iterator.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system.hpp>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>  // IWYU pragma: keep
#include <boost/beast.hpp>

#include <cerrno>
#include <chrono>
#include <cstdint>    // for std::uint64_t
#include <exception>  // for std::exception_ptr
#include <filesystem>
#include <fstream>
#include <functional>  // for std::ref
#include <iterator>    // for std::cbegin
#include <limits>      // for std::numeric_limits
#include <print>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move

auto
do_download(const std::string &host, const std::string &port,
            const std::string &target, const std::string &outfile,
            boost::asio::io_context &ioc,
            std::unordered_map<std::string, std::string> &header,
            boost::beast::error_code &ec, boost::asio::yield_context yield) {
  // ADS: this is the function that does the downloading called from
  // as asio io context. Also, look at these constants if bugs happen
  static constexpr auto http_version{11};
  static constexpr auto connect_timeout_seconds{10};
  static constexpr auto download_timeout_seconds{240};

  // attempt to open the file for output; do this prior to any async
  // ops starting
  boost::beast::http::file_body::value_type body;
  body.open(outfile.data(), boost::beast::file_mode::write, ec);
  if (ec) {
    return;
  }

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::beast::tcp_stream stream(ioc);

  // lookup server endpoint
  auto const results = resolver.async_resolve(host, port, yield[ec]);
  if (ec) {
    std::filesystem::remove(outfile);
    return;
  }

  // set the timeout for connecting
  stream.expires_after(std::chrono::seconds(connect_timeout_seconds));

  // connect to server
  stream.async_connect(results, yield[ec]);
  if (ec) {
    std::filesystem::remove(outfile);
    return;
  }

  // Set up an HTTP GET request message
  boost::beast::http::request<boost::beast::http::string_body> req{
    boost::beast::http::verb::get, target, http_version};
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // set the timeout for download (need a message for this)
  stream.expires_after(std::chrono::seconds(download_timeout_seconds));

  // send request to server
  boost::beast::http::async_write(stream, req, yield[ec]);
  if (ec) {
    std::filesystem::remove(outfile);
    return;
  }

  // make the response using the file-body
  boost::beast::http::response<boost::beast::http::file_body> res{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(boost::beast::http::status::ok, http_version),
  };

  boost::beast::http::response_parser<boost::beast::http::file_body> p{
    std::move(res),
  };
  p.eager(true);
  p.body_limit(std::numeric_limits<std::uint64_t>::max());

  boost::beast::flat_buffer buffer;
  // read the http response
  boost::beast::http::async_read(stream, buffer, p, yield[ec]);
  if (ec) {
    std::filesystem::remove(outfile);
    return;
  }
  res = p.release();

  stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

  if (ec && ec != boost::beast::errc::not_connected) {
    std::filesystem::remove(outfile);
    return;
  }

  const auto make_string = [](const auto &x) {
    return (std::ostringstream() << x).str();
  };

  header.clear();
  header.emplace("Status", std::to_string(res.result_int()));
  header.emplace("Reason", make_string(res.result()));
  for (const auto &h : res.base())
    header.emplace(make_string(h.name_string()), make_string(h.value()));
}

[[nodiscard]]
auto
download(const std::string &host, const std::string &port,
         const std::string &target, const std::string &outdir_arg)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {
  const auto outdir = std::filesystem::path(outdir_arg);
  const auto outfile = outdir / std::filesystem::path(target).filename();

  std::error_code out_ec;
  {
    std::filesystem::path outdir_path(outdir);
    if (std::filesystem::exists(outdir_path) &&
        !std::filesystem::is_directory(outdir_path)) {
      out_ec = std::make_error_code(std::errc::file_exists);
      std::println("{}: {}", outdir.string(), out_ec.message());
      return {{}, out_ec};
    }
    if (!std::filesystem::exists(outdir_path)) {
      const bool made_dir = std::filesystem::create_directories(outdir, out_ec);
      if (!made_dir) {
        std::println("{}: {}", outdir.string(), out_ec.message());
        return {{}, out_ec};
      }
    }
    std::ofstream out_test(outfile);
    if (!out_test) {
      out_ec = std::make_error_code(std::errc(errno));
      std::println("{}: {}", outfile.string(), out_ec.message());
      return {{}, out_ec};
    }
    const bool is_removed = std::filesystem::remove(outfile, out_ec);
    if (!is_removed)
      return {{}, out_ec};
  }

  std::unordered_map<std::string, std::string> header;
  boost::beast::error_code ec;
  boost::asio::io_context ioc;
  boost::asio::spawn(ioc,
                     std::bind(&do_download, host, port, target, outfile,
                               std::ref(ioc), std::ref(header), std::ref(ec),
                               std::placeholders::_1),
                     // on completion, spawn will call this function
                     [](std::exception_ptr ex) {
                       if (ex)
                         std::rethrow_exception(ex);
                     });

  // NOTE: here is where it all happens
  ioc.run();

  if (!ec) {
    // make sure we have a status if the http had no error; "reason" is
    // deprecated since rfc7230
    const auto status_itr = header.find("Status");
    const auto reason_itr = header.find("Reason");
    if (status_itr == std::cend(header) || reason_itr == std::cend(header))
      ec = std::make_error_code(std::errc::invalid_argument);
  }

  return {header, ec};
}
