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

#include "command_server.hpp"

static constexpr auto about = R"(
start an xfrase server
)";

static constexpr auto description = R"(
An xfrase server transfers methylation features to clients. The server
must be provided with one directory for methylomes and one directory
for cpg indexes. The methylome directory must include pairs of
methylome data and metadata files as produced by the 'format'
command. The indexes directory must include pairs of cpg index data
and metadata files as produced by the 'index' command. For each
methylome in the methylomes directory, the corresponding index must be
present in the indexes directory. For example, if a methylome was
analyzed using human reference hg38, then an index for hg38 must be
available. Note: the hostname or ip address for the server needs to be
used exactly by the client. If the server is started using 'localhost'
as the hostname, it will not be reachable by any remote client. The
server can run in detached mode.
)";

static constexpr auto examples = R"(
Examples:

xfrase server -s localhost -m methylomes -x indexes
)";

#include "arguments.hpp"
#include "config_file_utils.hpp"  // write_config_file
#include "logger.hpp"
#include "server.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <boost/describe.hpp>
#include <boost/program_options.hpp>

#include <cstdint>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

struct server_argset : argset_base<server_argset> {
  static constexpr auto default_config_filename = "xfrase_server_config.toml";

  static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_xfrase_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path(config_dir) / default_config_filename;
  }

  static constexpr auto hostname_default{"localhost"};
  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{xfrase_log_level::info};
  static constexpr auto n_threads_default{1};
  static constexpr auto max_resident_default = 32;
  std::string hostname{};
  std::string port{};
  std::string methylome_dir{};
  std::string index_dir{};
  std::string log_filename{};
  xfrase_log_level log_level{};
  std::uint32_t n_threads{};
  std::uint32_t max_resident{};
  bool daemonize{};
  std::string config_out{};

  auto
  log_options_impl() const {
    log_args<xfrase_log_level::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"hostname", std::format("{}", hostname)},
        {"port", std::format("{}", port)},
        {"methylome_dir", std::format("{}", methylome_dir)},
        {"index_dir", std::format("{}", index_dir)},
        {"log_filename", std::format("{}", log_filename)},
        {"log_level", std::format("{}", log_level)},
        {"n_threads", std::format("{}", n_threads)},
        {"max_resident", std::format("{}", max_resident)},
        {"daemonize", std::format("{}", daemonize)},
        // clang-format on
      });
  }

  [[nodiscard]] auto
  set_common_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Command line or config file");
    opts.add_options()
      // clang-format off
      ("hostname,s", value(&hostname)->default_value(hostname_default),
       "server hostname")
      ("port,p", value(&port)->default_value(port_default), "server port")
      ("daemonize,d", po::bool_switch(&daemonize), "daemonize the server")
      ("methylome-dir,m", value(&methylome_dir)->required(), "methylome directory")
      ("index-dir,x", value(&index_dir)->required(), "cpg index file directory")
      ("max-resident,r",
       value(&max_resident)->default_value(max_resident_default),
       "max resident methylomes")
      ("threads,t", value(&n_threads)->default_value(n_threads_default),
       "number of threads")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "log level {debug,info,warning,error,critical}")
      ("log-file,l", value(&log_filename)->value_name("console"),
       "log file name")
      // clang-format on
      ;
    return opts;
  }

  [[nodiscard]] auto
  set_cli_only_opts_impl() -> boost::program_options::options_description {
    boost::program_options::options_description opts("Command line only");
    // clang-format off
    opts.add_options()
      ("help,h", "print this message and exit")
      ("config-file,c",
       boost::program_options::value(&config_file)
       ->value_name("[arg]")->implicit_value(get_default_config_file(), ""),
       "use this config file")
      ("make-config",
       boost::program_options::value(&config_out),
       "write specified configuration to this file and exit")
      ;
    // clang-format on
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(server_argset, (), (
  hostname,
  port,
  methylome_dir,
  index_dir,
  log_filename,
  log_level,
  n_threads,
  max_resident,
  daemonize
)
)
// clang-format on

auto
command_server_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "server";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  server_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  if (!args.config_out.empty())
    return write_config_file(args) ? EXIT_FAILURE : EXIT_SUCCESS;

  std::shared_ptr<std::ostream> log_file =
    args.log_filename.empty()
      ? std::make_shared<std::ostream>(std::cout.rdbuf())
      : std::make_shared<std::ofstream>(args.log_filename, std::ios::app);

  logger &lgr = logger::instance(log_file, command, args.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  args.methylome_dir = std::filesystem::canonical(args.methylome_dir, ec);
  if (ec || args.methylome_dir.empty()) {
    lgr.error("Failed to get canonical dir for {}: {}", args.methylome_dir, ec);
    return EXIT_FAILURE;
  }

  if (args.daemonize) {
    auto s =
      server(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.index_dir, args.max_resident, lgr, ec, args.daemonize);
    if (ec) {
      lgr.error("Failure daemonizing server: {}.", ec);
      return EXIT_FAILURE;
    }
    s.run();
  }
  else {
    auto s =
      server(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.index_dir, args.max_resident, lgr, ec);
    s.run();
  }

  return EXIT_SUCCESS;
}
