#pragma once
#include <chrono>
#include <memory>

#define TIME(x) auto TIME_##x = std::chrono::high_resolution_clock::now();
#define MEASURE(a, b) std::chrono::duration_cast<std::chrono::microseconds>( TIME_##b - TIME_##a ).count() / 1000.f

constexpr auto order_relaxed = std::memory_order::memory_order_relaxed;
