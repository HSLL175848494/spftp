#ifndef HSLL_LOG
#define HSLL_LOG

#include <iostream>

/**
 * Macro for logging information with specified level
 * @param level Log level to use
 */
#define HSLL_LOGINFO(level, ...) \
    LogInfo(level, __VA_ARGS__);

/**
 * Macro for logging information with specified level
 * @param level Log level to use
 * @param func Function to call after logging
 */
#define HSLL_FUNC_LOGINFO(level, func, ...) \
    {                                       \
        LogInfo(level, __VA_ARGS__);        \
        func;                               \
    }

/**
 * Macro for conditional logging
 * @param exp Expression to evaluate
 * @param level Log level to use
 */
#define HSLL_EXP_LOGINFO(exp, level, ...) \
    {                                     \
        if (exp)                          \
            LogInfo(level, __VA_ARGS__);  \
    }

/**
 * Macro for conditional logging with additional function call
 * @param exp Expression to evaluate
 * @param func Function to call if expression is true
 * @param level Log level to use
 */
#define HSLL_EXP_FUNC_LOGINFO(exp, func, level, ...) \
    {                                                \
        if (exp)                                     \
        {                                            \
            LogInfo(level, __VA_ARGS__);             \
            func;                                    \
        }                                            \
    }

namespace HSLL
{
    /**
     * Enumeration for log levels
     */
    enum LOG_LEVEL
    {
        LOG_LEVEL_INFO,    // Informational messages
        LOG_LEVEL_WARNING, // Warning messages
        LOG_LEVEL_CRUCIAL, // crucial message
        LOG_LEVEL_ERROR,   // Error messages
    };

    /**
     * Utility method for logging information
     * Uses variadic templates to support multiple arguments
     * @param level Log level to use
     * @param ts Variadic template parameters to log
     * Note: A thread-safe logging library is recommended instead
     */
    template <class... TS>
    static void LogInfo(LOG_LEVEL level, TS... ts)
    {
#if defined(_DEBUG) || defined(DEBUG_)
        if (sizeof...(TS))
            (std::cout << ... << ts) << std::endl;
#else
        if (level > 1)
        {
            if (sizeof...(TS))
                (std::cout << ... << ts) << std::endl;
        }
#endif
    }
}

#endif