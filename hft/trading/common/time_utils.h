/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:34
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-02-03 16:56:50
 * @FilePath: /my_HFT/hft/common/time_utils.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <string>
#include <chrono>
#include <ctime>

namespace Common {
  typedef int64_t Nanos;

  // 时间戳类型 (纳秒精度)
	using TimeStamp = uint64_t;

  constexpr Nanos NANOS_TO_MICROS = 1000;
  constexpr Nanos MICROS_TO_MILLIS = 1000;
  constexpr Nanos MILLIS_TO_SECS = 1000;
  constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
  constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

	// 获取当前时间戳(纳秒)
	inline TimeStamp now_ns() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}

  inline auto getCurrentNanos() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  inline auto& getCurrentTimeStr(std::string* time_str) {
    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    time_str->assign(ctime(&time));
    if(!time_str->empty())
      time_str->at(time_str->length()-1) = '\0';
    return *time_str;
  }
}
