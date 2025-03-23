#ifndef HSLL_COROUTINE
#define HSLL_COROUTINE

#include <coroutine>
#include <optional>

namespace HSLL
{
    /**
     * @brief Enum to control coroutine initial suspension state
     */
    enum START_FLAG
    {
        START_FLAG_SUSPENDED = true,  //!< Coroutine starts in suspended state
        START_FLAG_NOSUSPEND = false  //!< Coroutine starts immediately
    };

    /**
     * @brief Suspend type selector based on START_FLAG
     * @tparam Suspend Flag indicating suspension state
     */
    template <START_FLAG Suspend>
    struct Suspend_Type;

    /**
     * @brief Specialization for suspended initial state
     */
    template <>
    struct Suspend_Type<START_FLAG_SUSPENDED>
    {
        using value = std::suspend_always;  //!< Always suspend coroutine
    };

    /**
     * @brief Specialization for non-suspended initial state
     */
    template <>
    struct Suspend_Type<START_FLAG_NOSUSPEND>
    {
        using value = std::suspend_never;  //!< Never suspend coroutine
    };

    /**
     * @brief Generator coroutine template with start flag and type parameter
     * @tparam FLAG Start flag controlling initial suspension
     * @tparam Args Variadic template parameters
     */
    template <START_FLAG FLAG, typename... Args>
    class Generator;

    /**
     * @brief Generator coroutine template specialization with return type
     * @tparam FLAG Start flag controlling initial suspension
     * @tparam T Return type of the generator
     */
    template <START_FLAG FLAG, class T>
    class Generator<FLAG, T>
    {
    public:
        /**
         * @brief Promise type defining coroutine behavior
         */
        struct promise_type
        {
            std::optional<T> optional;  //!< Optional value to hold return value

            /**
             * @brief Create return object from coroutine handle
             * @return Generator object bound to the coroutine handle
             */
            auto get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }

            /**
             * @brief Set return value for the coroutine
             * @param value Value to return
             */
            void return_value(T value) { optional = value; }

            /**
             * @brief Handle uncaught exceptions
             */
            void unhandled_exception() { std::terminate(); }

            /**
             * @brief Initial suspension state based on flag
             * @return Suspension object
             */
            Suspend_Type<FLAG>::value initial_suspend() { return {}; }

            /**
             * @brief Final suspension before coroutine destruction
             * @return Always suspend
             */
            std::suspend_always final_suspend() noexcept { return {}; }

            /**
             * @brief Yield value during coroutine execution
             * @param value Value to yield
             * @return Suspension object
             */
            std::suspend_always yield_value(T value)
            {
                optional = value;
                return {};
            }
        };

    private:
        std::coroutine_handle<promise_type> handle;  //!< Coroutine handle

    public:
        Generator() : handle(nullptr) {}  //!< Default constructor
        Generator(std::coroutine_handle<promise_type> handle) : handle(handle) {}  //!< Construct from handle

        ~Generator()
        {
            Destroy();  //!< Destroy coroutine on destruction
        }

        /**
         * @brief Destroy coroutine and release resources
         */
        void Destroy()
        {
            if (handle)
            {
                handle.destroy();
                handle = nullptr;
            }
        }

        /**
         * @brief Check if coroutine handle is valid
         * @return True if handle is valid
         */
        bool HandleInvalid()
        {
            return handle ? true : false;
        }

        /**
         * @brief Check if coroutine is done executing
         * @return True if done
         */
        bool hasDone()
        {
            if (handle)
                return handle.done();
            throw;  //!< Throw exception if handle is invalid
        }

        /**
         * @brief Resume coroutine execution
         * @return True if resumed successfully
         */
        bool Resume()
        {
            if (handle && !handle.done())
            {
                handle.resume();
                return true;
            }
            throw;  //!< Throw exception if handle is invalid or done
        }

        /**
         * @brief Get next value from coroutine
         * @return Optional value containing next result or nullopt if done
         */
        std::optional<T> next()
        {
            if (!handle.done())
            {
                handle.resume();
                return handle.promise().optional;
            }
            else
                return std::nullopt;
        }

        /**
         * @brief Get current value from coroutine
         * @return Optional value containing current result
         */
        std::optional<T> Value()
        {
            if (handle)
                return handle.promise().optional;
            else
                return std::nullopt;
        }

        /**
         * @brief Move assignment operator
         * @param other Generator to move
         * @return Reference to this Generator
         */
        Generator &operator=(Generator &&other)
        {
            if (this != &other)
            {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = nullptr;
            }
            return *this;
        }

        // Deleted copy constructors and assignment operators
        Generator(const Generator &) = delete;
        Generator &operator=(const Generator &) = delete;
        Generator(Generator &&other) = delete;
    };

    /**
     * @brief Generator coroutine template specialization without return type
     * @tparam FLAG Start flag controlling initial suspension
     */
    template <START_FLAG FLAG>
    class Generator<FLAG>
    {
    public:
        /**
         * @brief Promise type defining coroutine behavior
         */
        struct promise_type
        {
            /**
             * @brief Create return object from coroutine handle
             * @return Generator object bound to the coroutine handle
             */
            Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }

            /**
             * @brief Return void from coroutine
             */
            void return_void() {}

            /**
             * @brief Handle uncaught exceptions
             */
            void unhandled_exception() { std::terminate(); }

            /**
             * @brief Initial suspension state based on flag
             * @return Suspension object
             */
            Suspend_Type<FLAG>::value initial_suspend() { return {}; }

            /**
             * @brief Final suspension before coroutine destruction
             * @return Always suspend
             */
            std::suspend_always final_suspend() noexcept { return {}; }
        };

    private:
        std::coroutine_handle<promise_type> handle;  //!< Coroutine handle

    public:
        Generator() : handle(nullptr) {}  //!< Default constructor
        Generator(std::coroutine_handle<promise_type> handle) : handle(handle) {}  //!< Construct from handle

        ~Generator()
        {
            Destroy();  //!< Destroy coroutine on destruction
        }

        /**
         * @brief Destroy coroutine and release resources
         */
        void Destroy()
        {
            if (handle)
            {
                handle.destroy();
                handle = nullptr;
            }
        }

        /**
         * @brief Check if coroutine handle is valid
         * @return True if handle is valid
         */
        bool HandleInvalid()
        {
            return handle ? true : false;
        }

        /**
         * @brief Check if coroutine is done executing
         * @return True if done
         */
        bool hasDone()
        {
            if (handle)
                return handle.done();
            throw;  //!< Throw exception if handle is invalid
        }

        /**
         * @brief Resume coroutine execution
         * @return True if resumed successfully
         */
        bool Resume()
        {
            if (handle && !handle.done())
            {
                handle.resume();
                return true;
            }
            throw;  //!< Throw exception if handle is invalid or done
        }

        /**
         * @brief Move assignment operator
         * @param other Generator to move
         * @return Reference to this Generator
         */
        Generator &operator=(Generator &&other)
        {
            if (this != &other)
            {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = nullptr;
            }
            return *this;
        }

        // Deleted copy constructors and assignment operators
        Generator(const Generator &) = delete;
        Generator &operator=(const Generator &) = delete;
        Generator(Generator &&other) = delete;
    };
}

#endif