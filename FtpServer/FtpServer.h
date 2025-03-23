/**
 * @brief FTP Server class
 * @details Implements an FTP server with support for both active and passive data connections,
 *          user authentication, file transfers, and common FTP commands
 */
#ifndef HSLL_FTPSERVER
#define HSLL_FTPSERVER

#include <set>
#include <vector>
#include <cstring>
#include <errno.h>
#include <sys/stat.h>

#include "../Event/Eventcplus.h"
#include "../ThreadPool/ThreadPool.hpp"
#include "../Coroutine/Coroutine.hpp"

namespace HSLL
{
    /**
     * @brief Server configuration information container
     * @details Stores static server configuration parameters loaded from config file
     */
    struct ServerInfo
    {
        static bool utf8;                                           //!< Whether UTF-8 is supported
        static bool anonymous;                                      //!< Anonymous access enable flag
        static unsigned int rwtimeout;                              //!< I/O timeout in seconds
        static unsigned short port;                                 //!< Server listening port
        static char dir[1024];                                      //!< Root directory path
        static char ip[INET_ADDRSTRLEN];                            //!< Server IP address string
        static char encoding[32];                                   //!< The current system character encoding
        static std::set<std::pair<std::string, std::string>> users; //!< Valid user credentials set

        /**
         * @brief Load server configuration from file
         * @param configPath Path to configuration file
         * @return true if configuration loaded successfully, false otherwise
         */
        static bool LoadConfig(const char *configPath);
    };

    /**
     * @brief Main FTP server implementation class
     * @details Handles client connections, command processing, and data transfers
     */
    class FTPServer
    {
    public:
        /**
         * @brief Constructor with event buffer
         * @param evb Initialized event buffer for network operations
         * @param info Connection information structure
         */
        FTPServer(EVBuffer evb, ConnectionInfo info);

        ~FTPServer();

        /**
         * @brief Handle read event from client
         * @details Processes incoming data and triggers command parsing
         */
        void DealRead();

        /**
         * @brief Handle write event to client
         * @details Sends pending responses to client
         */
        void DealWrite();

        /**
         * @brief Handle new connection acceptance
         * @details Sends initial welcome message
         */
        void DealAccept();

        /**
         * @brief Send buffered data and enable write monitoring
         */
        void Send_And_EnableWR();

        /**
         * @brief Enable read/write event monitoring
         */
        void EnableRW();

        /**
         * @brief Disable read/write event monitoring
         */
        void DisableRW();

        /**
         * @brief Check if server instance is available for new operations
         * @return true if available, false otherwise
         */
        bool CheakFree();

        /**
         * @brief Check if error occurred in processing
         * @return true if error detected, false otherwise
         */
        bool CheakError();

    private:
        /// Data connection mode enumeration
        enum DataConnectionMode
        {
            DATA_MODE_NONE,   //!< No active data connection
            DATA_MODE_ACTIVE, //!< Active mode data connection
            DATA_MODE_PASSIVE //!< Passive mode data connection
        };

        EVBuffer evb;           //!< Underlying event buffer object
        ConnectionInfo info;    //!< Connection information structure
        std::string sWaitParse; //!< Buffer for incoming data awaiting parsing
        std::string sWaitSend;  //!< Buffer for outgoing data awaiting transmission

        bool utf8;       //!< Specifies whether UTF8 is enabled
        bool error;      //!< Error state flag
        bool certified;  //!< Client authentication status flag
        bool enableFree; //!< Flag indicating availability for new operations

        std::string user;           //!< Current authenticated username
        std::string clientIP;       //!< Client IP address for active mode
        std::string currentDir;     //!< Current working directory path
        std::string renameFromPath; //!< Temporary storage for RNFR command path

        int dataSocket;              //!< Active data connection socket
        int pasvSocket;              //!< Passive mode listening socket
        int clientPort;              //!< Client port for active mode connections
        DataConnectionMode dataMode; //!< Current data connection mode

        Generator<START_FLAG::START_FLAG_NOSUSPEND> task; //!< Coroutine task handler

        /**
         * @brief Handle LIST/NLST command (directory listing)
         * @return Generator for coroutine management
         */
        Generator<START_FLAG::START_FLAG_NOSUSPEND> HandleList();

        /**
         * @brief Handle file download (RETR command)
         * @param param Filename parameter from client
         * @return Generator for coroutine management
         */
        Generator<START_FLAG::START_FLAG_NOSUSPEND> HandleDownload(const std::string &param);

        /**
         * @brief Handle file upload (STOR command)
         * @param param Filename parameter from client
         * @return Generator for coroutine management
         */
        Generator<START_FLAG::START_FLAG_NOSUSPEND> HandleUpload(const std::string &param);

        /**
         * @brief Read data from network buffer
         */
        void Read();

        /**
         * @brief Send data from output buffer
         * @return true if all data sent successfully, false otherwise
         */
        bool Send();

        /**
         * @brief Parse incoming command buffer
         * @return true if parsing successful, false on protocol errors
         */
        bool Parse();

        /**
         * @brief Process individual FTP command
         * @param cmd FTP command string
         * @param param Command parameters
         * @return true if command processed, false if requires coroutine continuation
         */
        bool ProcessCommand(std::string &cmd, std::string &param);

        /**
         * @brief Handle PORT command (active mode setup)
         * @param param Port command parameters
         */
        void HandlePORT(const std::string &param);

        /**
         * @brief Handle PASV command (passive mode setup)
         */
        void HandlePASV();

        /**
         * @brief Manage coroutine task execution
         * @return true if task completed, false if needs continuation
         */
        bool DealTask();

        /**
         * @brief Establish data connection based on current mode
         * @return true if connection established successfully
         */
        bool EstablishDataConnection();

        /**
         * @brief Close active data connections
         */
        void CloseDataConnection();

        /**
         * @brief Set socket timeout parameters
         * @param socket Target socket descriptor
         * @param seconds Timeout in seconds
         */
        void SetSocketTimeout(int socket, int seconds);
    };
}
#endif