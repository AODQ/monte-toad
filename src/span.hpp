#pragma once

#include <type_traits>

// until GCC-10 gets distributed or something, idk

template <typename ElementType>
struct span {
private:
  ElementType * ptr = nullptr;
  std::size_t length = 0;
public:
  using element_type = ElementType;
  using value_type = typename std::remove_cv<ElementType>::type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = element_type *;
  using const_pointer = element_type const *;
  using reference = element_type &;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr span() noexcept {}

  span(pointer ptr_, size_t length_) noexcept : ptr(ptr_), length(length_) {}
  /* template <typename Container> span(Container const & ctr) noexcept : */
  /*   ptr(ctr.data()), length(ctr.size()) */
  /* {} */

  ~span() noexcept = default;
  span& operator=(const span& other) noexcept = default;

  constexpr size_type size() const noexcept { return length; }
  constexpr pointer  data()  const noexcept { return ptr; }

  constexpr bool empty() const noexcept { return length == 0; }

  constexpr reference front() const noexcept { return *ptr; }
  constexpr reference back()  const noexcept { return *(ptr + length - 1); }
  constexpr reference operator[](size_t idx) const noexcept {
    return *(ptr + idx);
  }

  constexpr iterator begin() const noexcept { return ptr; }
  constexpr iterator end()   const noexcept { return ptr + length; }
  constexpr const_iterator cbegin() const noexcept { return ptr; }
  constexpr const_iterator cend()   const noexcept { return ptr + length; }
  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(ptr + length);
  }
  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(ptr);
  }
  constexpr reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(ptr + length);
  }
  constexpr reverse_iterator crend() const noexcept {
    return const_reverse_iterator(ptr);
  }
  friend constexpr iterator begin(span s) noexcept { return s.begin(); }
  friend constexpr iterator end  (span s) noexcept { return s.end(); }
};

template <typename Container>
inline span<typename Container::value_type> make_span(Container & container) {
  return span<typename Container::value_type>(container.data(), container.size());
}

template <typename Container>
inline span<const typename Container::value_type>
  make_span(Container const & container)
{
  return span<const typename Container::value_type>(container.data(), container.size());
}
