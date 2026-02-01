#ifndef HPERF_RESErrorTypeLResult_HPP
#define HPERF_RESErrorTypeLResult_HPP

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "hperf/util/hperf_error.hpp"

namespace hperf {

template <typename ResultType, typename ErrorType>
class BasicResult;

/**
 * @brief It's alias for BasicResultType, equals to BasicResult<ResultType,hperf::HperfError>
 */
template <typename ResultType>
using HperfResult = BasicResult<ResultType, hperf::HperfError>;

/**
 * @brief Factory function, return BasicResult, in which is a result. The deafult is HperfResult, which is BasicResult<ResultType,hperf::HperfError>
 * @param args The arguments that construct the result.
 * @return A BasicResult object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline BasicResult<ResultType, ErrorType> make_result(Args&&... args) {
  return BasicResult<ResultType, ErrorType>{
      std::in_place_type<ResultType>,
      std::forward<Args>(args)...};
}

/**
 * @brief Factory function, return BasicResult, in which is a result. The deafult is HperfResult, which is BasicResult<ResultType,hperf::HperfError>
 * @param result_ptr The unique ptr of the result.
 * @return A BasicResult object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline BasicResult<ResultType, ErrorType> make_result(std::unique_ptr<ResultType> result_ptr) {
  return BasicResult<ResultType, ErrorType>{std::move(result_ptr)};
}

/**
 * @brief Factory function, return BasicResult, in which is an error. The deafult is HperfResult, which is BasicResult<ResultType,hperf::HperfError>
 * @param args The arguments that construct the error.
 * @return A BasicResult object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline BasicResult<ResultType, ErrorType> make_error_result(Args&&... args) {
  return BasicResult<ResultType, ErrorType>{
      std::in_place_type<ErrorType>,
      std::forward<Args>(args)...};
}

/**
 * @brief Factory function, return BasicResult, in which is an error. The deafult is HperfResult, which is BasicResult<ResultType,hperf::HperfError>
 * @param result_ptr The unique ptr of the error.
 * @return A BasicResult object
 */
template <typename ResultType, typename ErrorType = HperfError, typename... Args>
inline BasicResult<ResultType, ErrorType> make_error_result(std::unique_ptr<ErrorType> error_ptr) {
  return BasicResult<ResultType, ErrorType>{std::move(error_ptr)};
}

template <typename ResultType, typename ErrorType>
class BasicResult {
 public:
  BasicResult(const ResultType& result);
  BasicResult(ResultType&& result);
  BasicResult(std::unique_ptr<ResultType> result_ptr);
  template <typename... Args>
  BasicResult(std::in_place_type_t<ResultType>, Args&&... args);

  BasicResult(const ErrorType& error);
  BasicResult(ErrorType&& error);
  BasicResult(std::unique_ptr<ErrorType> error_ptr);
  template <typename... Args>
  BasicResult(std::in_place_type_t<ErrorType>, Args&&... args);

  BasicResult(BasicResult&&) = default;
  BasicResult& operator=(BasicResult&&) = default;

  bool ok() const;
  std::optional<ResultType> get_result() &&;
  std::optional<std::unique_ptr<ResultType>> get_result_ptr() &&;
  std::optional<ErrorType> get_error() &&;
  std::optional<std::unique_ptr<ErrorType>> get_error_ptr() &&;

 private:
  std::variant<std::unique_ptr<ResultType>, std::unique_ptr<ErrorType>> payload_;
};

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(const ResultType& result)
    : payload_(std::make_unique<ResultType>(result)) {}

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(ResultType&& result)
    : payload_(std::make_unique<ResultType>(std::move(result))) {}

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(std::unique_ptr<ResultType> result_ptr)
    : payload_(std::move(result_ptr)) {}

template <typename ResultType, typename ErrorType>
template <typename... Args>
BasicResult<ResultType, ErrorType>::BasicResult(std::in_place_type_t<ResultType>, Args&&... args)
    : payload_(std::make_unique<ResultType>(std::forward<Args>(args)...)) {}

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(const ErrorType& error)
    : payload_(std::make_unique<ErrorType>(error)) {}

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(ErrorType&& error)
    : payload_(std::make_unique<ErrorType>(std::move(error))) {}

template <typename ResultType, typename ErrorType>
BasicResult<ResultType, ErrorType>::BasicResult(std::unique_ptr<ErrorType> error_ptr)
    : payload_(std::move(error_ptr)) {}

template <typename ResultType, typename ErrorType>
template <typename... Args>
BasicResult<ResultType, ErrorType>::BasicResult(std::in_place_type_t<ErrorType>, Args&&... args)
    : payload_(std::make_unique<ErrorType>(std::forward<Args>(args)...)) {}

template <typename ResultType, typename ErrorType>
bool BasicResult<ResultType, ErrorType>::ok() const {
  return this->payload_.index() == 0;
}

template <typename ResultType, typename ErrorType>
std::optional<ResultType> BasicResult<ResultType, ErrorType>::get_result() && {
  if (std::unique_ptr<ResultType>* result = std::get_if<std::unique_ptr<ResultType>>(&payload_)) {
    return std::optional<ResultType>(std::move(**result));
  }
  return std::nullopt;
}

template <typename ResultType, typename ErrorType>
std::optional<std::unique_ptr<ResultType>> BasicResult<ResultType, ErrorType>::get_result_ptr() && {
  if (std::unique_ptr<ResultType>* result = std::get_if<std::unique_ptr<ResultType>>(&payload_)) {
    return std::optional<std::unique_ptr<ResultType>>(std::move(*result));
  }
  return std::nullopt;
}

template <typename ResultType, typename ErrorType>
std::optional<ErrorType> BasicResult<ResultType, ErrorType>::get_error() && {
  if (std::unique_ptr<ErrorType>* error =
          std::get_if<std::unique_ptr<ErrorType>>(&payload_)) {
    return std::optional<ErrorType>(std::move(**error));
  }
  return std::nullopt;
}

template <typename ResultType, typename ErrorType>
std::optional<std::unique_ptr<ErrorType>> BasicResult<ResultType, ErrorType>::get_error_ptr() && {
  if (std::unique_ptr<ErrorType>* error = std::get_if<std::unique_ptr<ErrorType>>(&payload_)) {
    return std::optional<std::unique_ptr<ErrorType>>(std::move(*error));
  }
  return std::nullopt;
}

}  // namespace hperf

#endif
