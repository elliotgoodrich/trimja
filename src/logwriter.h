// MIT License
//
// Copyright (c) 2025 Elliot Goodrich
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

#ifndef TRIMJA_LOGWRITER
#define TRIMJA_LOGWRITER

#include "logentry.h"

namespace trimja {

/**
 * @class LogWriter
 * @brief A class to write the contents of a .ninja_log file.
 */
class LogWriter {
 public:
  /**
   * @brief Constructs a LogWriter with the given output stream.  Note that
   * `out` must outlive the LogWriter.
   *
   * @param out The output stream to write the build log to.
   */
  explicit LogWriter(std::ostream& out, int version);

  /**
   * @brief Records a log entry to the output stream.
   *
   * @param path The entry to record.
   */
  void recordEntry(const LogEntry& entry);

 private:
  std::ostream* m_out;
};

}  // namespace trimja

#endif  // TRIMJA_LOGWRITER
