#pragma once

/*
 * literals.h
 *
 *  Created on: May 12, 2014
 *      Author: Jan Ole Vollmer
 */

namespace hyrise {

inline constexpr unsigned long long int operator"" _T (unsigned long long int value) {
  return value * 1024u * 1024u * 1024u * 1024u;
}

inline constexpr unsigned long long int operator"" _G (unsigned long long int value) {
  return value * 1024u * 1024u * 1024u;
}

inline constexpr unsigned long long int operator"" _M (unsigned long long int value) {
  return value * 1024u * 1024u;
}

inline constexpr unsigned long long int operator"" _K (unsigned long long int value) {
  return value * 1024u;
}

}  // namespace hyrise

