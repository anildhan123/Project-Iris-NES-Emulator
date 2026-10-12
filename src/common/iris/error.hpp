#pragma once

#include <optional>
#include <string>
#include <utility>

namespace iris {

enum class ErrorCode {
    NotFound,
    IoFailure,
    ParseFailure,
    ValidationFailure,
};

struct Error {
    ErrorCode code = ErrorCode::ValidationFailure;
    std::string message;
};

template <typename T>
class Result {
public:
    static Result success(T value) {
        Result result;
        result.value_ = std::move(value);
        return result;
    }

    static Result failure(ErrorCode code, std::string message) {
        Result result;
        result.error_ = Error{code, std::move(message)};
        return result;
    }

    [[nodiscard]] bool ok() const { return value_.has_value(); }

    [[nodiscard]] const T& value() const { return *value_; }

    [[nodiscard]] const Error& error() const { return error_; }

private:
    std::optional<T> value_;
    Error error_{};
};

}  // namespace iris
