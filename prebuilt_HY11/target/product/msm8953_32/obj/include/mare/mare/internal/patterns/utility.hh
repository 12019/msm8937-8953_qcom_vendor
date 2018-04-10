// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <tuple>

#include <mare/internal/task/functiontraits.hh>
#include <mare/internal/util/templatemagic.hh>

namespace mare {

namespace internal {

namespace pattern {

namespace utility {

constexpr size_t invalid_pos = std::numeric_limits<size_t>::max();

MARE_GCC_IGNORE_BEGIN("-Weffc++");
template<typename F, class Enable = void>
struct is_cpu_kernel : std::integral_constant<bool, false> {};

template<typename F>
struct is_cpu_kernel<F, typename std::enable_if<!std::is_same<typename function_traits<F>::kind,
                                                              user_code_types::invalid>::value>::type>
                     : std::integral_constant<bool, true> {};

template<typename F>
struct is_cpu_kernel<::mare::cpu_kernel<F>, void> : std::integral_constant<bool, true> {};

template<typename F, class Enable = void>
struct is_gpu_kernel : std::integral_constant<bool, false> {};

template<typename... KernelArgs>
struct is_gpu_kernel<::mare::gpu_kernel<KernelArgs...>, void> : std::integral_constant<bool, true> {};

template<typename F, class Enable = void>
struct is_hexagon_kernel : std::integral_constant<bool, false> {};

template<typename F>
struct is_hexagon_kernel<::mare::hexagon_kernel<F>, void> : std::integral_constant<bool, true> {};
MARE_GCC_IGNORE_END("-Weffc++");

template<size_t pos, typename Tuple>
struct search_cpu_kernel {
  using prior = search_cpu_kernel<pos - 1, Tuple>;
  template<size_t Pos>
  using ktype = typename std::tuple_element<Pos, Tuple>::type;

  static MARE_CONSTEXPR_CONST size_t prior_cpu_pos = prior::cpu_pos;
  static MARE_CONSTEXPR_CONST size_t current_cpu_pos = is_cpu_kernel<ktype<pos>>::value ? pos : invalid_pos;

  static_assert(prior::cpu_pos == invalid_pos || current_cpu_pos == invalid_pos,
                "Multiple cpu kernel defined!");

  static MARE_CONSTEXPR_CONST size_t cpu_pos = prior_cpu_pos != invalid_pos ? prior_cpu_pos : current_cpu_pos;
};

template<typename Tuple>
struct search_cpu_kernel<invalid_pos, Tuple> {
  static MARE_CONSTEXPR_CONST size_t cpu_pos = invalid_pos;
};

template<typename Tuple>
struct cpu_kernel_pos {
  using result = search_cpu_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

  static MARE_CONSTEXPR_CONST size_t pos = result::cpu_pos;
};

template<size_t pos, typename Tuple>
struct search_gpu_kernel {
  using prior = search_gpu_kernel<pos - 1, Tuple>;
  template<size_t Pos>
  using ktype = typename std::tuple_element<Pos, Tuple>::type;

  static MARE_CONSTEXPR_CONST size_t prior_gpu_pos = prior::gpu_pos;
  static MARE_CONSTEXPR_CONST size_t current_gpu_pos = is_gpu_kernel<ktype<pos>>::value ? pos : invalid_pos;

  static_assert(prior::gpu_pos == invalid_pos || current_gpu_pos == invalid_pos,
                "Multiple gpu kernel defined!");

  static MARE_CONSTEXPR_CONST size_t gpu_pos = prior_gpu_pos != invalid_pos ? prior_gpu_pos : current_gpu_pos;
};

template<typename Tuple>
struct search_gpu_kernel<invalid_pos, Tuple> {
  static MARE_CONSTEXPR_CONST size_t gpu_pos = invalid_pos;
};

template<typename Tuple>
struct gpu_kernel_pos {
  using result = search_gpu_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

  static MARE_CONSTEXPR_CONST size_t pos = result::gpu_pos;
};

template<size_t pos, typename Tuple>
struct search_hex_kernel {
  using prior = search_hex_kernel<pos - 1, Tuple>;
  template<size_t Pos>
  using ktype = typename std::tuple_element<Pos, Tuple>::type;

  static MARE_CONSTEXPR_CONST size_t prior_hex_pos = prior::hex_pos;
  static MARE_CONSTEXPR_CONST size_t current_hex_pos = is_hexagon_kernel<ktype<pos>>::value ? pos : invalid_pos;

  static_assert(prior::hex_pos == invalid_pos || current_hex_pos == invalid_pos,
                "Multiple hexagon kernel defined!");

  static MARE_CONSTEXPR_CONST size_t hex_pos = prior_hex_pos != invalid_pos ? prior_hex_pos : current_hex_pos;
};

template<typename Tuple>
struct search_hex_kernel<invalid_pos, Tuple> {
  static MARE_CONSTEXPR_CONST size_t hex_pos = invalid_pos;
};

template<typename Tuple>
struct hexagon_kernel_pos {
  using result = search_hex_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

  static MARE_CONSTEXPR_CONST size_t pos = result::hex_pos;
};

MARE_GCC_IGNORE_BEGIN("-Weffc++");
 template <typename T>
 struct is_input_buffer_ptr;

 template <typename T>
 struct is_input_buffer_ptr : public std::false_type {};

 template <typename T>
 struct is_input_buffer_ptr<::mare::buffer_ptr<const T>> : public std::true_type {};

 template <typename T>
 struct is_input_buffer_ptr<::mare::in<::mare::buffer_ptr<T>>> : public std::true_type {};

 template <typename T>
 struct is_output_buffer_ptr;

 template <typename T>
 struct is_output_buffer_ptr : public std::false_type {};

 template <typename T>
 struct is_output_buffer_ptr<::mare::buffer_ptr<const T>> : public std::false_type {};

 template <typename T>
 struct is_output_buffer_ptr<::mare::buffer_ptr<T>> : public std::true_type {};

 template <typename T>
 struct is_output_buffer_ptr<::mare::out<::mare::buffer_ptr<T>>> : public std::true_type {};

 template <typename T>
 struct is_output_buffer_ptr<::mare::inout<::mare::buffer_ptr<T>>> : public std::true_type {};
MARE_GCC_IGNORE_END("-Weffc++");

 template<typename Tuple, size_t index>
 struct num_input_buffer_helper {
  enum { value = (is_input_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value ? 1 : 0) +
                  num_input_buffer_helper<Tuple, index-1>::value };
 };

 template<typename Tuple>
 struct num_input_buffer_helper<Tuple, 0> {
  enum { value = 0 };
 };

 template<typename Tuple>
 struct num_input_buffer_in_tuple {
  enum { value = num_input_buffer_helper<Tuple, std::tuple_size<Tuple>::value>::value };
 };

template<typename Tuple, size_t index>
struct num_output_buffer_helper {
  enum { value = (is_output_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value ? 1 : 0) +
                  num_output_buffer_helper<Tuple, index-1>::value };
};

template<typename Tuple>
struct num_output_buffer_helper<Tuple, 0> {
  enum { value = 0 };
};

template<typename Tuple>
struct num_output_buffer_in_tuple {
  enum { value = num_output_buffer_helper<Tuple, std::tuple_size<Tuple>::value>::value };
};

template<typename Tuple>
struct get_num_buffer {
  enum { value = num_output_buffer_in_tuple<Tuple>::value + num_input_buffer_in_tuple<Tuple>::value };
};

template <typename Tuple, size_t index>
int get_buffer_size(Tuple& t,
                    typename std::enable_if<
                    !is_input_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value &&
                    !is_output_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value
                    >::type* = nullptr)
{
  MARE_UNUSED(t);
  return -1;
};

template <typename Tuple, size_t index>
int get_buffer_size(Tuple& t,
                    typename std::enable_if<
                    is_input_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value ||
                    is_output_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value
                    >::type* = nullptr)
{
  return std::get<index-1>(t).size();
};

template<size_t n, typename Tuple>
struct buffer_size_check_helper
{
  static int check_size(Tuple& t) {
    int sz = get_buffer_size<Tuple, n>(t);
    int prev_sz = buffer_size_check_helper<n-1, Tuple>::check_size(t);
    MARE_API_THROW(sz == -1 || prev_sz == -1 || sz == prev_sz, "Buffer size mismatch!");
    return prev_sz;
  }
};

template<typename Tuple>
struct buffer_size_check_helper<1, Tuple>
{
  static int check_size(Tuple& t) {
    int sz = get_buffer_size<Tuple, 1>(t);
    return sz;
  }
};

template<typename Tuple>
void buffer_size_check(Tuple& t)
{
  buffer_size_check_helper<std::tuple_size<Tuple>::value, Tuple>::check_size(t);
}

template<size_t... Indices, typename Fn, typename Tuple>
void tuple_unpack(Fn&& fn, size_t first, size_t stride, size_t last,
                  Tuple&& t, integer_list_gen<Indices...>)
{
  fn(first, stride, last, std::get<Indices-1>(std::forward<Tuple>(t))...);
}

template<typename Fn, typename ...Args>
void fn_call_wrapper(Fn&& fn,
                     size_t first, size_t stride, size_t last,
                     std::tuple<Args...>& t)
{
  tuple_unpack(std::forward<Fn>(fn), first, stride, last, t, typename integer_list<sizeof...(Args)>::type());
}

template<typename ArgType, typename T>
ArgType&& arg_transform(ArgType&& arg, mare::buffer_ptr<T>& out_buf,
                        typename std::enable_if<
                        !is_output_buffer_ptr<typename std::remove_reference<ArgType>::type>::value>
                        ::type* = nullptr){
  MARE_UNUSED(out_buf);
  return std::forward<ArgType>(arg);
}

template<typename ArgType, typename T>
mare::buffer_ptr<T>&& arg_transform(ArgType&& arg, mare::buffer_ptr<T>& out_buf,
                                  typename std::enable_if<
                                  is_output_buffer_ptr<typename std::remove_reference<ArgType>::type>::value>
                                  ::type* = nullptr)
{
  MARE_UNUSED(arg);
  return std::forward<buffer_ptr<T>>(out_buf);
}

template<size_t index, typename Tuple>
struct extract_output_buf
{
  static constexpr size_t value =
    is_output_buffer_ptr<
    typename std::remove_reference<typename std::tuple_element<index-1, Tuple>::type>::type>::value ?
    index-1 : extract_output_buf<index-1, Tuple>::value;
};

template<typename Tuple>
struct extract_output_buf<0, Tuple>
{
  static constexpr size_t value = invalid_pos;
};

template <size_t N, typename T>
void bas_add(buffer_acquire_set<N>& bas, T& ptr,
             typename std::enable_if<is_input_buffer_ptr<T>::value>::type* = nullptr)
{
  bas.add(ptr, bufferpolicy::action_t::acquire_r);
}

template <size_t N, typename T>
void bas_add(buffer_acquire_set<N>& bas, T& ptr,
             typename std::enable_if<is_output_buffer_ptr<T>::value>::type* = nullptr)
{
  bas.add(ptr, bufferpolicy::action_t::acquire_w);
}

template <size_t N, typename T>
void bas_add(buffer_acquire_set<N>&, T&,
             typename std::enable_if<!is_input_buffer_ptr<T>::value && !is_output_buffer_ptr<T>::value>
             ::type* = nullptr)
{}

template <size_t pos, size_t N, typename Tuple>
struct process_buffer_in_args {
  static void add_buffer(buffer_acquire_set<N>& bas, Tuple& tp) {
    bas_add(bas, std::get<pos-1>(tp));
    process_buffer_in_args<pos-1, N, Tuple>::add_buffer(bas, tp);
  }
};

template <size_t N, typename Tuple>
struct process_buffer_in_args<0, N, Tuple> {
  static void add_buffer(buffer_acquire_set<N>&, Tuple&)
  {}
};

template <typename Buffer>
struct buffer_data_type {};

template <typename T>
struct buffer_data_type<mare::buffer_ptr<T>> {
  using data_type = T;
};

};
};
};
};
