/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:34
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-05 16:54:57
 * @FilePath: /my_HFT/hft/common/macros.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstring>
#include <iostream>

/// Branch prediction hints.
#ifdef LIKELY
#undef LIKELY
#endif
#define LIKELY(x) __builtin_expect(!!(x), 1)

#ifdef UNLIKELY  
#undef UNLIKELY
#endif
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/// Check condition and exit if not true.
inline auto ASSERT_MSG(bool cond, const std::string &msg) noexcept {
  if (UNLIKELY(!cond)) {
    std::cerr << "ASSERT : " << msg << std::endl;

    exit(EXIT_FAILURE);
  }
}

inline auto FATAL(const std::string &msg) noexcept {
  std::cerr << "FATAL : " << msg << std::endl;

  exit(EXIT_FAILURE);
}
