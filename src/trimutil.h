// MIT License
//
// Copyright (c) 2024 Elliot Goodrich
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef TRIMJA_TRIMUTIL
#define TRIMJA_TRIMUTIL

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <type_traits>

namespace trimja {

namespace detail {
class Imp;
}  // namespace detail

/**
 * @enum TrimOptions
 * @brief Bit flags controlling trimja's optional diagnostic output.
 *
 * The flags are independent and combined with bitwise OR, so any of the four
 * combinations can be requested, e.g.
 * `TrimOptions::Explain | TrimOptions::Stats`.
 */
enum class TrimOptions : std::uint8_t {
  None = 0,
  Explain = 1 << 0,  ///< Print to stderr why each part of the build file was
                     ///< kept.
  Stats = 1 << 1,    ///< Print to stderr a summary of how much build time was
                     ///< trimmed away.
};

constexpr TrimOptions& operator|=(TrimOptions& lhs, TrimOptions rhs) {
  using U = std::underlying_type_t<TrimOptions>;
  lhs = static_cast<TrimOptions>(static_cast<U>(lhs) | static_cast<U>(rhs));
  return lhs;
}

constexpr TrimOptions operator|(TrimOptions lhs, TrimOptions rhs) {
  return lhs |= rhs;
}

constexpr TrimOptions& operator&=(TrimOptions& lhs, TrimOptions rhs) {
  using U = std::underlying_type_t<TrimOptions>;
  lhs = static_cast<TrimOptions>(static_cast<U>(lhs) & static_cast<U>(rhs));
  return lhs;
}

constexpr TrimOptions operator&(TrimOptions lhs, TrimOptions rhs) {
  return lhs &= rhs;
}

constexpr TrimOptions operator~(TrimOptions value) {
  using U = std::underlying_type_t<TrimOptions>;
  return static_cast<TrimOptions>(~static_cast<U>(value));
}

/**
 * @class TrimUtil
 * @brief Utility to trim a Ninja build file based on a list of affected files.
 */
class TrimUtil {
  std::unique_ptr<detail::Imp> m_imp;

 public:
  /**
   * @brief Default constructor for TrimUtil.
   */
  TrimUtil();

  /**
   * @brief Destructor for TrimUtil.
   */
  ~TrimUtil();

  /**
   * @brief Trims the given Ninja build file based on the affected files.
   *
   * @param output The output stream to write the trimmed Ninja file to.
   * @param ninjaFile The path to the original Ninja build file.
   * @param ninjaFileContents The contents of the original Ninja build file.
   * @param affected The input stream containing the list of affected files.
   * @param targets If non-empty, restricts the output to only build commands
   * that are needed (transitively) to build one of these targets. Build
   * commands that are not reachable from any target are removed entirely
   * rather than turned into a `phony` command, so that requesting them from
   * ninja fails. An empty string is a reserved sentinel (no valid ninja
   * output path can be empty) meaning "everything listed in the input
   * file's own `default` statement"; if the input file has no `default`
   * statement then, just as plain `ninja` builds everything when there is
   * no `default` statement, the sentinel resolves to "everything" rather
   * than being an error.
   * @param options A combination of `TrimOptions` flags controlling the
   * optional diagnostic output printed to stderr (whether to `--explain` why
   * each build command was kept and whether to print the summary of how much
   * build time was trimmed).  Neither affects the trimmed build file written
   * to @p output.  Warnings about affected paths that could not be found are
   * always printed regardless of these flags.
   */
  void trim(std::ostream& output,
            const std::filesystem::path& ninjaFile,
            const std::string& ninjaFileContents,
            std::istream& affected,
            std::span<const std::string> targets,
            TrimOptions options);
};

}  // namespace trimja

#endif  // TRIMJA_TRIMUTIL
