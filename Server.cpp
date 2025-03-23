
#include "FtpServer/FtpTask.hpp"

using namespace HSLL;

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        if (ServerInfo::LoadConfig("config") == false)
            return -1;
    }
    else if (argc == 3)
    {
        if (strcmp(argv[1], "-config") != 0)
        {
            HSLL_LOGINFO(LOG_LEVEL_ERROR, "Invalid command line arguments")
            return -1;
        }

        if (ServerInfo::LoadConfig(argv[2]) == false)
            return -1;
    }
    else
    {
        HSLL_LOGINFO(LOG_LEVEL_ERROR, "Invalid command line arguments")
        return -1;
    }

    EVSocket *socket = EVSocket::Construct(ServerInfo::port);

    if (socket->SetService(FTPConnection, FTPDisconnection, FTPRead, FTPWrite) != 0)
        return -1;
    if (socket->Listen() != 0)
        return -1;
    if (socket->SetSignalExit(SIGINT) != 0)
        return -1;

    pool.Init(10000, 6);

    HSLL_LOGINFO(LOG_LEVEL_CRUCIAL, "The server is ready to start")

    if (socket->EventLoop() != 0)
        return -1;

    pool.Exit();
    socket->Release();

    HSLL_LOGINFO(LOG_LEVEL_CRUCIAL, "Exit success")
    return 0;
}