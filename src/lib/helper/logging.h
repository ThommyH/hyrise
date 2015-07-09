#pragma once

/*
 * logging.h
 *
 *  Created on: May 19, 2014
 *      Author: Jan Ole Vollmer
 */

#include <log4cxx/logger.h>

#define LOG4CXX_THROW(logger, ExceptionType, message) { \
        ::log4cxx::helpers::MessageBuffer oss_; \
        auto messageStr = oss_.str(oss_ << message); \
        if (logger->isErrorEnabled()) {\
        	logger->forcedLog(::log4cxx::Level::getError(), messageStr, LOG4CXX_LOCATION); \
        }\
        throw_<ExceptionType>(messageStr); \
      }

namespace hyrise {

template <typename T>
constexpr auto takesMessage() -> decltype(T(std::string()), bool()) {
  return true;
}

template <typename T>
constexpr auto takesMessage() -> decltype(T(), bool()) {
  return false;
}

template <typename T>
auto throw_(const std::string& message) -> typename std::enable_if<takesMessage<T>(), bool>::type {
  throw T(message);
}

template <typename T>
auto throw_(const std::string& message) -> typename std::enable_if<not takesMessage<T>(), bool>::type {
  throw T();
}

}  // namespace hyrise
