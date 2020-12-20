#pragma once
#include <chrono>

#define TIME(x) auto TIME_##x = std::chrono::high_resolution_clock::now();
#define MEASURE(a, b) std::chrono::duration_cast<std::chrono::microseconds>( TIME_##b - TIME_##a ).count() / 1000.f
