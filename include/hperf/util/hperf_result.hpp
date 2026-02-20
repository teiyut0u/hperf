#ifndef HPERF_RESErrorTypeLResult_HPP
#define HPERF_RESErrorTypeLResult_HPP

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "hperf/util/hperf_error.hpp"

namespace hperf {

template <typename ResultType, typename ErrorType>
class Result;

/**
 * @brief alias, equals to Result<ResultType,hperf::HperfError>
 */
template <typename ResultType>
using HperfResult = Result<ResultType, hperf::HperfError>;

/**
 * @brief Factory function, return Result, in which is a result. The deafult is HperfResult, which is Result<ResultType,hperf::HperfError>
 * @param args The arguments that construct the result.
 * @return A Result object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline Result<ResultType, ErrorType> make_result(Args&&... args) {
  return Result<ResultType, ErrorType>{
      std::in_place_type<ResultType>,
      std::forward<Args>(args)...};
}

/**
 * @brief Factory function, return Result, in which is a result. The deafult is HperfResult, which is Result<ResultType,hperf::HperfError>
 * @param result_ptr The unique ptr of the result.
 * @return A Result object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline Result<ResultType, ErrorType> make_result(std::unique_ptr<ResultType> result_ptr) {
  return Result<ResultType, ErrorType>{std::move(result_ptr)};
}

/**
 * @brief Factory function, return Result, in which is an error. The deafult is HperfResult, which is Result<ResultType,hperf::HperfError>
 * @param args The arguments that construct the error.
 * @return A Result object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline Result<ResultType, ErrorType> make_error_result(Args&&... args) {
  return Result<ResultType, ErrorType>{
      std::in_place_type<ErrorType>,
      std::forward<Args>(args)...};
}

/**
 * @brief Factory function, return Result, in which is an error. The deafult is HperfResult, which is Result<ResultType,hperf::HperfError>
 * @param result_ptr The unique ptr of the error.
 * @return A Result object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline Result<ResultType, ErrorType> make_error_result(std::unique_ptr<ErrorType> error_ptr) {
  return Result<ResultType, ErrorType>{std::move(error_ptr)};
}

template <typename ResultType, typename ErrorType>
class Result {
 public:
  explicit Result() = default;
  explicit Result(const ResultType& result);
  explicit Result(ResultType&& result);
  explicit Result(std::unique_ptr<ResultType>&& result_ptr);
  template <typename... Args>
  explicit Result(std::in_place_type_t<ResultType>, Args&&... args);

  explicit Result(const ErrorType& error);
  explicit Result(ErrorType&& error);
  explicit Result(std::unique_ptr<ErrorType>&& error_ptr);
  template <typename... Args>
  explicit Result(std::in_place_type_t<ErrorType>, Args&&... args);

  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool ok() const;
  std::optional<ResultType> get_result() &&;
  std::unique_ptr<ResultType> get_result_ptr() &&;
  std::optional<ErrorType> get_error() &&;
  std::unique_ptr<ErrorType> get_error_ptr() &&;

 private:
  std::variant<std::unique_ptr<ResultType>, std::unique_ptr<ErrorType>> payload_;
};

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(const ResultType& result)
    : payload_(std::make_unique<ResultType>(result)) {}

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(ResultType&& result)
    : payload_(std::make_unique<ResultType>(std::move(result))) {}

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(std::unique_ptr<ResultType>&& result_ptr)
    : payload_(std::move(result_ptr)) {}

template <typename ResultType, typename ErrorType>
template <typename... Args>
Result<ResultType, ErrorType>::Result(std::in_place_type_t<ResultType>, Args&&... args)
    : payload_(std::make_unique<ResultType>(std::forward<Args>(args)...)) {}

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(const ErrorType& error)
    : payload_(std::make_unique<ErrorType>(error)) {}

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(ErrorType&& error)
    : payload_(std::make_unique<ErrorType>(std::move(error))) {}

template <typename ResultType, typename ErrorType>
Result<ResultType, ErrorType>::Result(std::unique_ptr<ErrorType>&& error_ptr)
    : payload_(std::move(error_ptr)) {}

template <typename ResultType, typename ErrorType>
template <typename... Args>
Result<ResultType, ErrorType>::Result(std::in_place_type_t<ErrorType>, Args&&... args)
    : payload_(std::make_unique<ErrorType>(std::forward<Args>(args)...)) {}

template <typename ResultType, typename ErrorType>
bool Result<ResultType, ErrorType>::ok() const {
  return this->payload_.index() == 0;
}

template <typename ResultType, typename ErrorType>
std::optional<ResultType> Result<ResultType, ErrorType>::get_result() && {
  if (auto* p = this->get_result_ptr()) {
    return *p;
  } else {
    return std::nullopt;
  }
}

template <typename ResultType, typename ErrorType>
std::unique_ptr<ResultType> Result<ResultType, ErrorType>::get_result_ptr() && {
  if (std::holds_alternative<std::unique_ptr<ResultType>>(payload_)) {
    return std::get<std::unique_ptr<ResultType>>(std::move(payload_));
  } else {
    return nullptr;
  }
}

template <typename ResultType, typename ErrorType>
std::optional<ErrorType> Result<ResultType, ErrorType>::get_error() && {
  if (auto* p = this->get_error_ptr()) {
    return *p;
  } else {
    return std::nullopt;
  }
}

template <typename ResultType, typename ErrorType>
std::unique_ptr<ErrorType> Result<ResultType, ErrorType>::get_error_ptr() && {
  if (std::holds_alternative<std::unique_ptr<ErrorType>>(payload_)) {
    return std::get<std::unique_ptr<ErrorType>>(std::move(payload_));
  } else {
    return nullptr;
  }
}

}  // namespace hperf

#endif
