#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif
#include <Windows.h>
#include <Xinput.h>
#include <tchar.h>

#include <iostream>
#include <syncstream>
#include <sstream>
#include <fstream>

#include <string>
#include <vector>
#include <map>
#include <array>
#include <tuple>
#include <variant>

#include <algorithm>
#include <utility>

#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>

#include <locale>
#include <format>
#include <chrono>

#include <numbers>
#include <limits>

#include <ranges>
#include <type_traits>
#include <concepts>

#include <cassert>