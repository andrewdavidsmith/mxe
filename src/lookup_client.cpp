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

#include "lookup_client.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using std::array;
using std::cerr;
using std::format;
using std::ofstream;
using std::print;
using std::println;
using std::string;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;
using hr_clock = std::chrono::high_resolution_clock;

namespace asio = boost::asio;
namespace ph = asio::placeholders;
namespace bs = boost::system;
namespace po = boost::program_options;
using tcp = asio::ip::tcp;
using steady_timer = asio::steady_timer;

struct mc16_client {
  mc16_client(asio::io_context &io_context, const string &server,
              const string &port, request_header &req_hdr, request &req,
              bool verbose = false) :
    resolver(io_context), socket(io_context), deadline{socket.get_executor()},
    req_hdr{req_hdr}, req{std::move(req)},  // move b/c req can be big
    verbose{verbose} {
    // (1) call async, (2) set deadline, (3) register check_deadline
    resolver.async_resolve(
      server, port,
      std::bind(&mc16_client::handle_resolve, this, ph::error, ph::results));
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
    deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
  }

  auto handle_resolve(const bs::error_code &err,
                      const tcp::resolver::results_type &endpoints) -> void {
    if (!err) {
      asio::async_connect(
        socket, endpoints,
        std::bind(&mc16_client::handle_connect, this, ph::error));
      // ADS: set deadline after calling async op
      deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
    }
    else {
      status = err;
      if (verbose)
        println("Error resolving server: {}", status.message());
    }
  }

  void handle_connect(const bs::error_code &err) {
    deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      if (verbose)
        println("Connected to server: {}",
                boost::lexical_cast<string>(socket.remote_endpoint()));
      if (const auto req_hdr_compose{compose(req_buf, req_hdr)};
          !req_hdr_compose.error) {
        if (const auto req_body_compose = to_chars(
              req_hdr_compose.ptr, req_buf.data() + request_buf_size, req);
            !req_body_compose.error) {
          asio::async_write(
            socket,
            vector<asio::const_buffer>{
              asio::buffer(req_buf),
              asio::buffer(req.offsets),
            },
            std::bind(&mc16_client::handle_write_request, this, ph::error));
          deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
        }
        else {
          status = req_body_compose.error;
          if (verbose)
            println("Error forming request body: {}", req_body_compose.error);
        }
      }
      else {
        status = req_hdr_compose.error;
        if (verbose)
          println("Error forming request header: {}", req_hdr_compose.error);
      }
    }
    else {
      status = err;
      if (verbose)
        println("Error connecting: {}", err);
    }
  }

  auto handle_write_request(const bs::error_code &err) -> void {
    deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      asio::async_read(
        socket, asio::buffer(resp_buf),
        asio::transfer_exactly(response_buf_size),
        std::bind(&mc16_client::handle_read_response_header, this, ph::error));
      deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
    }
    else {
      status = err;
      if (verbose)
        println("Error writing request: {}", err.message());
    }
  }

  auto handle_read_response_header(const bs::error_code &err) -> void {
    // ADS: does this go here?
    deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      if (const auto resp_hdr_parse{parse(resp_buf, resp_hdr)};
          !resp_hdr_parse.error) {
        if (verbose)
          println("Response header: {}", resp_hdr.summary_serial());
        do_read_counts();
      }
      else {
        status = resp_hdr_parse.error;
        println("Received error: {}", resp_hdr_parse.error);
        do_finish(resp_hdr_parse.error);
      }
    }
    else {
      status = err;
      if (verbose)
        println("Error reading response header: {}", err.message());
    }
  }

  auto do_read_counts() -> void {
    // ADS: this 'resize' seems to belong with the response class
    resp.counts.resize(req.n_intervals);
    asio::async_read(socket, asio::buffer(resp.counts),
                     asio::transfer_exactly(resp.get_counts_n_bytes()),
                     std::bind(&mc16_client::do_finish, this, ph::error));
    // ADS: before or after the call to async?
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
  }

  auto do_finish(const std::error_code &err) -> void {
    deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      if (verbose)
        println("Completing transaction: {}", err.message());
    }
    else {
      status = err;
      if (verbose)
        println("Error reading counts: {}", err.message());
    }
  }

  auto check_deadline() -> void {
    if (!socket.is_open())
      return;

    if (deadline.expiry() <= steady_timer::clock_type::now()) {
      // deadline passed: close socket so remaining async ops are
      // cancelled (see below)
      if (verbose)
        println("Error deadline expired.");

      bs::error_code shutdown_ec;  // for non-throwing
      socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
      if (verbose)
        println("Shutdown status: {}", shutdown_ec);
      deadline.expires_at(steady_timer::time_point::max());

      /* ADS: closing here if needed?? */
      bs::error_code socket_close_ec;  // for non-throwing
      socket.close(socket_close_ec);
    }

    // wait again
    deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
  }

  tcp::resolver resolver;
  tcp::socket socket;
  steady_timer deadline;
  request_buffer req_buf;
  request_header req_hdr;
  request req;
  response_buffer resp_buf;
  response_header resp_hdr;
  response resp;
  std::error_code status;
  bool verbose{};
  uint32_t read_timeout_seconds{3};
};  // struct mc16_client

auto
lookup_client_main(int argc, char *argv[]) -> int {
  static constexpr auto default_port = "5000";
  static const auto description = "client";

  bool verbose{};
  bool debug{};

  string port{};
  string accession{};
  string index_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("hostname,H", po::value(&hostname)->required(), "hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "accession")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ("debug,d", po::bool_switch(&debug), "print debug info")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }
  verbose = verbose || debug;  // if debug, then verbose

  if (verbose)
    print("Arguments:\n"
          "Accession: {}\n"
          "Hostname: {}\n"
          "Port: {}\n"
          "Index file: {}\n"
          "Intervals file: {}\n"
          "Output file: {}\n",
          accession, hostname, port, index_file, intervals_file, output_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }
  if (debug)
    println("Index:\n{}", index);

  const auto gis = genomic_interval::load(index, intervals_file);
  if (gis.empty()) {
    println(cerr, "failed to read intervals file: {}", intervals_file);
    return EXIT_FAILURE;
  }
  if (verbose)
    print("Number of intervals: {}\n", size(gis));

  const auto get_offsets_start{hr_clock::now()};
  const vector<methylome::offset_pair> offsets = index.get_offsets(gis);
  const auto get_offsets_stop{hr_clock::now()};
  if (verbose)
    println("Elapsed time to get offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  request_header hdr{accession, index.n_cpgs_total, 0};
  request req{static_cast<uint32_t>(size(offsets)), offsets};

  asio::io_context io_context;
  mc16_client mc16c(io_context, hostname, port, hdr, req, debug);
  const auto client_start{hr_clock::now()};
  io_context.run();
  const auto client_stop{hr_clock::now()};

  if (verbose)
    println("Elapsed time for query: {:.3}s\n"
            "Response header: {}\n"
            R"(Transaction status: "{}")",
            duration(client_start, client_stop),
            mc16c.resp_hdr.summary_serial(), mc16c.status.message());

  if (mc16c.status)
    return EXIT_FAILURE;

  std::ofstream out(output_file);
  if (!out) {
    println("failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_intervals(out, index, gis, mc16c.resp.counts);
  const auto output_stop{hr_clock::now()};
  if (verbose)
    println("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
