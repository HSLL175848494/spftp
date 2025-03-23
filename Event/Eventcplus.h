#ifndef HSLL_EVENTCPLUS
#define HSLL_EVENTCPLUS

#include <map>
#include <signal.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include "../Log/Log.hpp"

/**
 * @brief Debug macro: Prints error codes in debug mode
 * @note Only active when _DEBUG or DEBUG_ is defined
 */
#if defined(_DEBUG) || defined(DEBUG_)
#define HSLL_SOCKET_PRINT_DEBUG(code) printf("%s\n", errorStrs[code]);
#else
#define HSLL_SOCKET_PRINT_DEBUG(code)
#endif

/**
 * @brief Error return macro with debug printing
 * @param code Error code to return
 */
#define HSLL_SOCKET_RET(code)         \
    {                                 \
        HSLL_SOCKET_PRINT_DEBUG(code) \
        return code;                  \
    }

/**
 * @brief Error check and return macro
 * @param exp Condition to check (returns when true)
 * @param code Error code to return when condition is true
 */
#define HSLL_SOCKET_ERROR_RET(exp, code)  \
    {                                     \
        if (exp)                          \
        {                                 \
            HSLL_SOCKET_PRINT_DEBUG(code) \
            return code;                  \
        }                                 \
    }

/**
 * @brief Error check with cleanup function and return macro
 * @param exp Condition to check (returns when true)
 * @param func Cleanup function to execute when condition is true
 * @param code Error code to return
 */
#define HSLL_SOCKET_ERROR_FUNC_RET(exp, func, code) \
    {                                               \
        if (exp)                                    \
        {                                           \
            func;                                   \
            HSLL_SOCKET_PRINT_DEBUG(code)           \
            return code;                            \
        }                                           \
    }

/**
 * @brief Get the last socket error description string
 */
#define HSLL_SOCKET_GET_ERROR \
    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR())

namespace HSLL
{
    /**
     * @brief Connection information structure containing IP address and port
     */
    struct ConnectionInfo
    {
        int port;                 //!< Port number in network byte order
        char ip[INET_ADDRSTRLEN]; //!< IP address string in dotted decimal format
    };

    /**
     * @brief Event buffer wrapper class providing data read/write interfaces
     * @details Encapsulates libevent's bufferevent, managing input/output buffers
     */
    class EVBuffer
    {
        friend class EVSocket;
        event_base *base;
        bufferevent *bev; //!< libevent bufferevent object
        evbuffer *input;  //!< Input buffer pointer
        evbuffer *output; //!< Output buffer pointer

    public:
        /**
         * @brief Read data from the input buffer
         * @param buf Destination buffer pointer
         * @param size Maximum number of bytes to read
         * @return Number of bytes actually read (>=0 on success), -1 on error
         */
        int Read(void *buf, unsigned int size);

        /**
         * @brief Write data to the output buffer
         * @param buf Source data pointer
         * @param size Number of bytes to write
         * @return 0 on success, -1 on failure
         */
        int Write(const void *buf, unsigned int size);

        /**
         * @brief Enable read and write events
         * @return 0 on success, -1 on failure
         */
        int EnableWR();

        /**
         * @brief Disable read and write events
         * @return 0 on success, -1 on failure
         */
        int DisableWR();

        /**
         * @brief Constructor (restricted to friend class)
         * @param bev Initialized bufferevent pointer
         * @param base Event base associated with the buffer
         */
        EVBuffer(bufferevent *bev, event_base *base);
    };

    // Forward declarations of callback function types
    typedef void *(*ConnectProc)(EVBuffer evb, ConnectionInfo info); //!< New connection callback
    typedef void (*CloseProc)(void *ctx);                            //!< Connection close callback
    typedef bool (*ReadProc)(void *ctx);                             //!< Data readable callback
    typedef bool (*WriteProc)(void *ctx);                            //!< Data writable callback

    /**
     * @brief Event-driven Socket core class
     * @details Encapsulates libevent network operations, providing TCP server functionality
     *          with asynchronous event handling
     */
    class EVSocket
    {
    private:
        friend class EVBuffer;

        unsigned short port;   //!< Listening port number
        unsigned short status; //!< Status flag (0:uninitialized 1:configured 2:running)

        event *evExit;              //!< Exit signal event object
        sockaddr_in sin;            //!< Address structure
        evconnlistener *listener;   //!< Connection listener
        evconnlistener_cb cbAccept; //!< Connection accept callback function

        // Static members
        static EVSocket *instance;            //!< Singleton instance pointer
        static ReadProc rp;                   //!< User read callback function pointer
        static WriteProc wp;                  //!< User write callback function pointer
        static CloseProc csp;                 //!< User close callback function pointer
        static ConnectProc cnp;               //!< User connect callback function pointer
        static event_base *base;              //!< libevent event base object
        static const char *const errorStrs[]; //!< Error code description array
        static std::map<bufferevent *, std::pair<ConnectionInfo,void *>> cnts; //!< Connections

        /**
         * @brief Extract connection information from socket address structure
         * @param address Source address structure pointer
         * @param info Destination connection info structure pointer
         */
        static void GetHostInfo(sockaddr *address, ConnectionInfo *info);

        /**
         * @brief Default connection accept callback (standard server mode)
         * @param listener Connection listener
         * @param fd File descriptor for the new connection
         * @param address Client address structure
         * @param socklen Address structure length
         * @param ctx User context (unused)
         */
        static void Callback_Accept(evconnlistener *listener, evutil_socket_t fd,
                                    sockaddr *address, int socklen, void *ctx);

        /**
         * @brief Default data read callback
         * @param bev Bufferevent pointer
         * @param ctx User context
         */
        static void Callback_Read(bufferevent *bev, void *ctx);

        /**
         * @brief Default data write callback
         * @param bev Bufferevent pointer
         * @param ctx User context
         */
        static void Callback_Write(bufferevent *bev, void *ctx);

        /**
         * @brief Default event callback (handles connection close and errors)
         * @param bev Bufferevent pointer
         * @param events Triggered event flags
         * @param ctx User context
         */
        static void Callback_Event(bufferevent *bev, short events, void *ctx);

        /**
         * @brief Default listener error callback
         * @param listener Connection listener
         * @param ctx User context (unused)
         */
        static void Callback_Error(evconnlistener *listener, void *ctx);

        /**
         * @brief Signal handler callback
         * @param sig Received signal
         * @param events Event type
         * @param ctx EVSocket instance pointer
         */
        static void Callback_Signal(evutil_socket_t sig, short events, void *ctx);

        /**
         * @brief Constructor (private, use Construct method to create instance)
         * @param port Listening port number
         * @param ip Binding IP address string (defaults to "0.0.0.0")
         */
        explicit EVSocket(unsigned short port, const char *ip);

    public:
        /**
         * @brief Release all resources
         */
        static void Release();

        /**
         * @brief Create EVSocket instance (singleton pattern)
         * @param port Listening port number
         * @param ip Binding IP address (defaults to "0.0.0.0")
         * @return Instance pointer on success, nullptr on failure
         */
        static EVSocket *Construct(unsigned short port, const char *ip = "0.0.0.0");

        /**
         * @brief Set service callback functions
         * @param cp Callback function for new connections
         * @param dcp Callback function for connection close
         * @param rp Callback function for readable data
         * @param wp Callback function for writable status
         * @return 0 on success, non-zero error code (see errorStrs)
         */
        unsigned int SetService(ConnectProc cp, CloseProc dcp, ReadProc rp, WriteProc wp);

        /**
         * @brief Start listening on the specified port
         * @return 0 on success, non-zero error code (see errorStrs)
         * @note SetService must be called first to configure callbacks
         */
        unsigned int Listen();

        /**
         * @brief Set exit signal handler
         * @param sg Signal to listen for (e.g., SIGINT)
         * @return 0 on success, non-zero error code (see errorStrs)
         */
        unsigned int SetSignalExit(int sg);

        /**
         * @brief Start the event loop
         * @return 0 on success, non-zero error code (see errorStrs)
         * @note This method blocks until the event loop exits
         */
        unsigned int EventLoop();

        /**
         * @brief Get error description message
         * @param code Error code
         * @return Corresponding error description string
         */
        const char *getLastError(int code);

        // Disable copy constructor and assignment operator
        EVSocket(const EVSocket &) = delete;
        EVSocket &operator=(const EVSocket &) = delete;
    };
}

#endif