#pragma once

#include <cstdarg>
#include <cstdint>

namespace cosmos::log {
    enum class Type : uint8_t {
        Debug,
        Info,
        Warning,
        Error,
    };

    void enable_display(bool delay = false);
    void disable_display();

    void enable_paging();

    void println_args(Type type, const char* file, uint32_t line, const char* fmt, va_list args);
    void println(Type type, const char* file, uint32_t line, const char* fmt, ...);

    const uint8_t* get_start();
    uint64_t get_size();
} // namespace cosmos::log

#define DEBUG(fmt, ...) cosmos::log::println(cosmos::log::Type::Debug, __FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)
#define INFO(fmt, ...) cosmos::log::println(cosmos::log::Type::Info, __FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)
#define WARN(fmt, ...) cosmos::log::println(cosmos::log::Type::Warning, __FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)
#define ERROR(fmt, ...) cosmos::log::println(cosmos::log::Type::Error, __FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)

#define DEBUG_ARGS(fmt, args) cosmos::log::println_args(cosmos::log::Type::Debug, __FILE__, __LINE__, fmt, args)
#define INFO_ARGS(fmt, args) cosmos::log::println_args(cosmos::log::Type::Info, __FILE__, __LINE__, fmt, args)
#define WARN_ARGS(fmt, args) cosmos::log::println_args(cosmos::log::Type::Warning, __FILE__, __LINE__, fmt, args)
#define ERROR_ARGS(fmt, args) cosmos::log::println_args(cosmos::log::Type::Error, __FILE__, __LINE__, fmt, args)
