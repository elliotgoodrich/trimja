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

#ifndef TRIMJA_INDEXINTO
#define TRIMJA_INDEXINTO

#include <cstddef>

namespace trimja {

/**
 * @def INDEXINTO_DEBUG_INFO
 * @brief Controls whether IndexInto stores a pointer to the container for
 * debugging and safety checks.
 *
 * If not explicitly defined, it is set to 1 in debug builds (when NDEBUG is not
 * defined), and 0 in release builds (when NDEBUG is defined).
 *
 * When INDEXINTO_DEBUG_INFO is 1, IndexInto includes a const CONTAINER* member.
 * When INDEXINTO_DEBUG_INFO is 0, IndexInto omits the container pointer for
 * efficiency.
 */
#ifndef INDEXINTO_DEBUG_INFO
#ifndef NDEBUG
#define INDEXINTO_DEBUG_INFO 1
#else
#define INDEXINTO_DEBUG_INFO 0
#endif
#endif

#if INDEXINTO_DEBUG_INFO

namespace detail {
/**
 * @brief IndexIntoDebug is a lightweight index wrapper for referencing
 * elements in a container.
 *
 * This class is designed to be used as a safe and efficient index type for
 * iterating or referencing elements within a container.  It stores a pointer
 * to the container for additional debugging assistance.
 *
 * @tparam CONTAINER The container type being indexed.
 */
template <typename CONTAINER>
class IndexIntoDebug {
  std::size_t m_index;
  const CONTAINER* m_container;

 public:
  using difference_type = std::ptrdiff_t;

  /**
   * @brief Default constructor. Initializes index to zero.
   *
   * In debug builds, also initializes the container pointer to nullptr.
   */
  IndexIntoDebug() : m_index{0}, m_container{nullptr} {}

  /**
   * @brief Constructs an IndexIntoDebug with a given index and container
   * pointer.
   *
   * @param index The index value.
   * @param container Pointer to the container.
   */
  IndexIntoDebug(std::size_t index, const CONTAINER* container)
      : m_index{index}, m_container{container} {}

  /**
   * @brief Returns the current index value.
   * @return The index value.
   */
  std::size_t index() const noexcept { return m_index; }

  /**
   * @brief Pre-increment operator. Advances the index by one.
   * @return Reference to this IndexIntoDebug.
   */
  IndexIntoDebug& operator++() noexcept {
    ++m_index;
    return *this;
  }

  /**
   * @brief Post-increment operator. Advances the index by one and returns the
   * previous value.
   * @return Copy of the IndexIntoDebug before increment.
   */
  IndexIntoDebug operator++(int) noexcept {
    IndexIntoDebug tmp = *this;
    ++*this;
    return tmp;
  }
};
}  // namespace detail

/**
 * @brief IndexInto is a lightweight index wrapper for referencing elements in a
 * container.
 *
 * This class is designed to be used as a safe and efficient index type for
 * iterating or referencing elements within a container. In debug builds, it can
 * optionally store a pointer to the container for additional safety or
 * debugging purposes.
 *
 * @tparam CONTAINER The container type being indexed.
 */
template <typename CONTAINER>
using IndexInto = detail::IndexIntoDebug<CONTAINER>;

#else  // INDEXINTO_DEBUG_INFO

namespace detail {
/**
 * @brief IndexIntoNoDebug is a lightweight index wrapper for referencing
 * elements in a container.
 *
 * This class is designed to be used as a safe and efficient index type for
 * iterating or referencing elements within a container.
 *
 * @tparam CONTAINER The container type being indexed.
 */
template <typename CONTAINER>
class IndexIntoNoDebug {
  std::size_t m_index;

 public:
  using difference_type = std::ptrdiff_t;

  /**
   * @brief Default constructor. Initializes index to zero.
   */
  IndexIntoNoDebug() : m_index{0} {}

  /**
   * @brief Constructs an IndexIntoNoDebug with a given index and container
   * pointer.
   *
   * @param index The index value.
   * @param container Pointer to the container - completely ignored.
   */
  IndexIntoNoDebug(std::size_t index, const void*) : m_index{index} {}

  /**
   * @brief Returns the current index value.
   * @return The index value.
   */
  std::size_t index() const noexcept { return m_index; }

  /**
   * @brief Pre-increment operator. Advances the index by one.
   * @return Reference to this IndexIntoNoDebug.
   */
  IndexIntoNoDebug& operator++() noexcept {
    ++m_index;
    return *this;
  }

  /**
   * @brief Post-increment operator. Advances the index by one and returns the
   * previous value.
   * @return Copy of the IndexIntoNoDebug before increment.
   */
  IndexIntoNoDebug operator++(int) noexcept {
    IndexIntoNoDebug tmp = *this;
    ++*this;
    return tmp;
  }
};

}  // namespace detail

/**
 * @brief IndexInto is a lightweight index wrapper for referencing elements in a
 * container.
 *
 * This class is designed to be used as a safe and efficient index type for
 * iterating or referencing elements within a container. In debug builds, it can
 * optionally store a pointer to the container for additional safety or
 * debugging purposes.
 *
 * @tparam CONTAINER The container type being indexed.
 */
template <typename CONTAINER>
using IndexInto = detail::IndexIntoNoDebug<CONTAINER>;

#endif  // INDEXINTO_DEBUG_INFO

/**
 * @brief Computes the difference between two IndexInto objects.
 * @param lhs Left-hand side IndexInto.
 * @param rhs Right-hand side IndexInto.
 * @return The difference in their index values.
 */
template <typename CONTAINER>
typename IndexInto<CONTAINER>::difference_type operator-(
    const IndexInto<CONTAINER>& lhs,
    const IndexInto<CONTAINER>& rhs) noexcept {
  using difference_type = typename IndexInto<CONTAINER>::difference_type;
  return static_cast<difference_type>(lhs.index()) -
         static_cast<difference_type>(rhs.index());
}

/**
 * @brief Equality comparison for IndexInto.
 * @param left First IndexInto.
 * @param right Second IndexInto.
 * @return Whether their indices are equal.
 */
template <typename CONTAINER>
bool operator==(const IndexInto<CONTAINER>& left,
                const IndexInto<CONTAINER>& right) noexcept {
  return left.index() == right.index();
}

/**
 * @brief Inequality comparison for IndexInto.
 * @param left First IndexInto.
 * @param right Second IndexInto.
 * @return Whether their indices are not equal.
 */
template <typename CONTAINER>
bool operator!=(const IndexInto<CONTAINER>& left,
                const IndexInto<CONTAINER>& right) noexcept {
  return left.index() != right.index();
}

/**
 * @brief IndexIntoIterator is a iterator for IndexInto values.
 *
 * @tparam INDEX The IndexInto type being incremented.
 */
template <typename INDEX>
class IndexIntoIterator {
  INDEX m_current;

 public:
  using value_type = INDEX;

  IndexIntoIterator(INDEX current) : m_current{current} {}

  value_type operator*() const { return m_current; }

  IndexIntoIterator& operator++() {
    ++m_current;
    return *this;
  }

  IndexIntoIterator operator++(int) {
    IndexIntoIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator==(const IndexIntoIterator& a,
                         const IndexIntoIterator& b) {
    return a.m_current == b.m_current;
  }

  friend bool operator!=(const IndexIntoIterator& a,
                         const IndexIntoIterator& b) {
    return !(a == b);
  }
};

/**
 * @brief IndexIntoRange is a range over a set of IndexInto values.
 *
 * @tparam INDEX The IndexInto type being held.
 */
template <typename INDEX>
class IndexIntoRange {
  INDEX m_first;
  INDEX m_last;

 public:
  IndexIntoRange(INDEX first, INDEX last) : m_first{first}, m_last{last} {}

  IndexIntoIterator<INDEX> begin() const { return m_first; }
  IndexIntoIterator<INDEX> end() const { return m_last; }
};

}  // namespace trimja

#endif  // TRIMJA_INDEXINTO
