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

#include "command_index.hpp"

static constexpr auto about = R"(
make an index for a given reference genome
)";

static constexpr auto description = R"(
The index is used to accelerate searches within methylomes and must be
created from the same reference genome that was used originally to map
the reads and generate the single-CpG methylation levels. The order of
chromosomes within the reference genome is not relevant as long as
each chromosome is correct. The index is in two files. The index data
is a binary file with size just over 100MB for the human genome and it
should have the extension '.cpg_idx'. The index metadata is a small
JSON format file (on a single line) that can easily be examined with
any JSON formatter (e.g., jq or json_pp).  These two files should
reside in the same directory and typically only the index data file is
specified when it is used.
)";

static constexpr auto examples = R"(
Examples:

xfrase index -v debug -x hg38.cpg_idx -g hg38.fa
)";

#include "cpg_index.hpp"
#include "cpg_index_meta.hpp"
#include "logger.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <boost/program_options.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <stdlib.h>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_index_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "index";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  std::string genome_filename{};
  std::string index_file{};
  xfrase_log_level log_level{};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("genome,g", po::value(&genome_filename)->required(), "genome_file")
    ("index,x", po::value(&index_file)->required(),
     std::format("output file (must end in {})", cpg_index::filename_extension).data())
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    lgr.error("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  const auto extension_found = std::filesystem::path(index_file).extension();
  if (extension_found != cpg_index::filename_extension) {
    lgr.error("Required filename extension {} (given: {})",
              cpg_index::filename_extension, extension_found);
    return EXIT_FAILURE;
  }

  const auto metadata_output = get_default_cpg_index_meta_filename(index_file);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Genome", genome_filename},
    {"Index", index_file},
    {"Index metadata", metadata_output},
    // clang-format on
  };
  log_args<xfrase_log_level::info>(args_to_log);

  const auto constr_start = std::chrono::high_resolution_clock::now();
  const auto [index, cim, err] = initialize_cpg_index(genome_filename);
  const auto constr_stop = std::chrono::high_resolution_clock::now();
  if (err) {
    if (err == std::errc::no_such_file_or_directory)
      lgr.error("Genome file not found: {}", genome_filename);
    else
      lgr.error("Error constructing index: {}", err);
    return EXIT_FAILURE;
  }
  lgr.debug("Index construction time: {:.3}s",
            duration(constr_start, constr_stop));

  if (const auto write_err = index.write(index_file); write_err) {
    lgr.error("Error writing cpg index {}: {}", index_file, write_err);
    return EXIT_FAILURE;
  }

  if (const auto write_err = cim.write(metadata_output); write_err) {
    lgr.error("Error writing cpg index metadata: {}", write_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
