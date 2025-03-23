#include <fcntl.h>
#include <sstream>
#include <iconv.h>
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include <langinfo.h>

#include "FtpServer.h"

namespace HSLL
{
    char ServerInfo::dir[1024];
    char ServerInfo::encoding[32]{};
    char ServerInfo::ip[INET_ADDRSTRLEN];
    bool ServerInfo::utf8 = false;
    bool ServerInfo::anonymous = false;
    unsigned int ServerInfo::rwtimeout = 5;
    unsigned short ServerInfo::port = 4567;
    std::set<std::pair<std::string, std::string>> ServerInfo::users;

    void trim(std::string &s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                        { return !std::isspace(ch); }));

        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
    }

    bool ServerInfo::LoadConfig(const char *configPath)
    {
        std::ifstream file(configPath);
        std::vector<std::string> lines;
        std::string line;
        bool has_ip = false;
        bool has_dir = false;
        size_t i = 0;

        if (!file.is_open())
        {
            HSLL_LOGINFO(LOG_LEVEL_ERROR, "Configuration file not found");
            return false;
        }

        while (std::getline(file, line))
            lines.push_back(line);

        while (i < lines.size())
        {
            line = lines[i];
            trim(line);
            if (line.empty() || line[0] == '#')
            {
                ++i;
                continue;
            }

            if (line.back() != ':')
                goto exitFalse;

            std::string param = line.substr(0, line.size() - 1);

            ++i;

            if (i >= lines.size())
                goto exitFalse;

            std::string value_line = lines[i];
            trim(value_line);
            if (value_line.empty() || value_line[0] != '$')
                goto exitFalse;

            std::string value = value_line.substr(1);
            trim(value);

            if (param == "ip")
            {
                struct in_addr addr;
                if (inet_pton(AF_INET, value.c_str(), &addr) != 1)
                    goto exitFalse;

                strncpy(ServerInfo::ip, value.c_str(), INET_ADDRSTRLEN - 1);
                ServerInfo::ip[INET_ADDRSTRLEN - 1] = '\0';
                has_ip = true;
                ++i;
            }
            else if (param == "dir")
            {
                if (value.size() >= sizeof(ServerInfo::dir))
                    goto exitFalse;

                strncpy(ServerInfo::dir, value.c_str(), sizeof(ServerInfo::dir) - 1);
                ServerInfo::dir[sizeof(ServerInfo::dir) - 1] = '\0';
                has_dir = true;
                ++i;
            }
            else if (param == "rwtimeout")
            {
                try
                {
                    size_t pos;
                    unsigned long num = std::stoul(value, &pos);

                    if (pos != value.size() || num > UINT_MAX)
                        goto exitFalse;

                    ServerInfo::rwtimeout = static_cast<unsigned int>(num);
                }
                catch (...)
                {
                    goto exitFalse;
                }
                ++i;
            }
            else if (param == "anonymous")
            {
                if (value == "true")
                {
                    ServerInfo::anonymous = true;
                }
                else if (value == "false")
                {
                    ServerInfo::anonymous = false;
                }
                else
                {
                    goto exitFalse;
                }
                ++i;
            }
            else if (param == "utf-8")
            {
                if (value == "true")
                {
                    ServerInfo::utf8 = true;
                }
                else if (value == "false")
                {
                    ServerInfo::utf8 = false;
                }
                else
                {
                    goto exitFalse;
                }
                ++i;
            }
            else if (param == "port")
            {
                try
                {
                    size_t pos;
                    unsigned long num = std::stoul(value, &pos);
                    if (pos != value.size() || num > USHRT_MAX)
                        goto exitFalse;

                    ServerInfo::port = static_cast<unsigned short>(num);
                }
                catch (...)
                {
                    goto exitFalse;
                }
                ++i;
            }
            else if (param == "users")
            {
                while (i < lines.size())
                {
                    std::string user_line = lines[i];
                    trim(user_line);
                    if (user_line.empty() || user_line[0] != '$')
                        break;

                    std::string user_entry = user_line.substr(1);
                    trim(user_entry);
                    size_t space_pos = user_entry.find(' ');
                    if (space_pos == std::string::npos || space_pos == 0 || space_pos == user_entry.size() - 1)
                        goto exitFalse;

                    std::string username = user_entry.substr(0, space_pos);
                    std::string password = user_entry.substr(space_pos + 1);
                    ServerInfo::users.insert(std::make_pair(username, password));
                    ++i;
                }
            }
            else
            {
                goto exitFalse;
            }
        }

        if (!has_ip || !has_dir)
            goto exitFalse;

        setlocale(LC_CTYPE, "");
        strcpy(ServerInfo::encoding, nl_langinfo(CODESET));
        return true;

    exitFalse:
        HSLL_LOGINFO(LOG_LEVEL_ERROR, "The configuration file is invalid");
        return false;
    }

    std::string ToUpperCase(const std::string &str)
    {
        std::string result = str;
        for (char &c : result)
        {
            if (islower(c))
            {
                c = (char)toupper(c);
            }
        }
        return result;
    }

    void FTPServer::SetSocketTimeout(int socket, int seconds)
    {
        struct timeval timeout;
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;

        if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        {
            HSLL_LOGINFO(LOG_LEVEL_WARNING, "Failed to set socket receive timeout");
        }

        if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        {
            HSLL_LOGINFO(LOG_LEVEL_WARNING, "Failed to set socket send timeout");
        }
    }

    std::string convertEncoding(const std::string &input, const std::string &fromEncoding, const std::string &toEncoding)
    {
        if (input.empty())
            return "";

        if (fromEncoding == toEncoding)
            return input;

        iconv_t cd = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
        if (cd == (iconv_t)-1)
            return input;

        char *in_buf = const_cast<char *>(input.data());
        size_t in_bytes_left = input.size();

        size_t out_bytes_left = in_bytes_left * 4;
        std::string output(out_bytes_left, '\0');
        char *out_buf = &output[0];

        size_t result = iconv(cd, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left);
        iconv_close(cd);

        if (result == (size_t)-1)
            return input;

        output.resize(output.size() - out_bytes_left);
        return output;
    }

    bool FTPServer::EstablishDataConnection()
    {
        if (dataMode == DATA_MODE_PASSIVE)
        {
            if (pasvSocket == -1)
                return false;

            struct timeval timeout = {ServerInfo::rwtimeout, 0};
            setsockopt(pasvSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            dataSocket = accept(pasvSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);

            close(pasvSocket);
            pasvSocket = -1;

            if (dataSocket == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return false;
            }
            SetSocketTimeout(dataSocket, ServerInfo::rwtimeout);
        }
        else if (dataMode == DATA_MODE_ACTIVE)
        {
            if (dataSocket == -1)
                return false;

            int flags = fcntl(dataSocket, F_GETFL, 0);
            fcntl(dataSocket, F_SETFL, flags | O_NONBLOCK);

            sockaddr_in clientAddr = {0};
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = htons((short)clientPort);
            inet_pton(AF_INET, clientIP.c_str(), &clientAddr.sin_addr);

            int connectResult = connect(dataSocket, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
            if (connectResult < 0 && errno != EINPROGRESS)
            {
                close(dataSocket);
                dataSocket = -1;
                return false;
            }

            if (connectResult < 0)
            {
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(dataSocket, &write_fds);

                struct timeval timeout = {ServerInfo::rwtimeout, 0};
                if (select(dataSocket + 1, NULL, &write_fds, NULL, &timeout) <= 0)
                {
                    close(dataSocket);
                    dataSocket = -1;
                    return false;
                }

                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(dataSocket, SOL_SOCKET, SO_ERROR, &error, &len) || error)
                {
                    close(dataSocket);
                    dataSocket = -1;
                    return false;
                }
            }

            fcntl(dataSocket, F_SETFL, flags);
            SetSocketTimeout(dataSocket, ServerInfo::rwtimeout);
        }
        else
        {
            return false;
        }
        return true;
    }

    void FTPServer::CloseDataConnection()
    {
        if (dataSocket != -1)
        {
            close(dataSocket);
            dataSocket = -1;
        }

        if (pasvSocket != -1)
        {
            close(pasvSocket);
            pasvSocket = -1;
        }
    }

    void FTPServer::HandlePORT(const std::string &param)
    {
        std::vector<int> values;
        std::stringstream ss(param);
        std::string item;

        while (std::getline(ss, item, ','))
        {
            values.push_back(atoi(item.c_str()));
        }

        if (values.size() != 6)
        {
            sWaitSend.append("501 Syntax error in parameters or arguments.\r\n");
            return;
        }

        char ip[32];
        sprintf(ip, "%d.%d.%d.%d", values[0], values[1], values[2], values[3]);
        int port = values[4] * 256 + values[5];

        if (dataSocket != -1)
        {
            close(dataSocket);
            dataSocket = -1;
        }

        dataSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket < 0)
        {
            sWaitSend.append("425 Can't open data connection.\r\n");
            return;
        }

        clientIP = ip;
        clientPort = port;
        dataMode = DATA_MODE_ACTIVE;
        sWaitSend.append("200 PORT command successful.\r\n");
    }

    void FTPServer::HandlePASV()
    {
        if (dataSocket != -1)
        {
            close(dataSocket);
            dataSocket = -1;
        }

        pasvSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (pasvSocket < 0)
        {
            sWaitSend.append("425 Can't open passive socket.\r\n");
            return;
        }

        int opt = 1;
        setsockopt(pasvSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = 0;

        if (bind(pasvSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(pasvSocket);
            pasvSocket = -1;
            sWaitSend.append("425 Can't bind passive socket.\r\n");
            return;
        }

        if (listen(pasvSocket, 1) < 0)
        {
            close(pasvSocket);
            pasvSocket = -1;
            sWaitSend.append("425 Can't listen on passive socket.\r\n");
            return;
        }

        socklen_t len = sizeof(addr);
        getsockname(pasvSocket, (struct sockaddr *)&addr, &len);
        int pasvPort = ntohs(addr.sin_port);

        std::vector<std::string> ipParts;
        std::stringstream ss(ServerInfo::ip);
        std::string item;
        while (std::getline(ss, item, '.'))
            ipParts.push_back(item);

        char pasvResponse[64];
        sprintf(pasvResponse, "%s,%s,%s,%s,%d,%d",
                ipParts[0].c_str(), ipParts[1].c_str(),
                ipParts[2].c_str(), ipParts[3].c_str(),
                pasvPort / 256, pasvPort % 256);

        dataMode = DATA_MODE_PASSIVE;
        sWaitSend.append("227 Entering Passive Mode (").append(pasvResponse).append(")\r\n");
    }

    Generator<START_FLAG::START_FLAG_NOSUSPEND> FTPServer::HandleList()
    {
        sWaitSend.append("150 Opening data connection.\r\n");

        while (!Send())
        {
            co_await std::suspend_always{};
            if (error)
                co_return;
        }

        if (!EstablishDataConnection())
        {
            sWaitSend.append("425 Can't open data connection.\r\n");
            co_return;
        }

        DIR *dir = opendir(currentDir.c_str());
        if (!dir)
        {
            sWaitSend.append("550 Failed to open directory.\r\n");
            CloseDataConnection();
            co_return;
        }

        std::string listing;
        struct dirent *entry;
        struct stat statBuf;
        char timeBuf[80];

        while ((entry = readdir(dir)))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            std::string fullPath = currentDir + "/" + entry->d_name;
            if (stat(fullPath.c_str(), &statBuf) != 0)
                continue;

            char perm[] = "----------";
            perm[0] = S_ISDIR(statBuf.st_mode) ? 'd' : '-';
            perm[1] = (statBuf.st_mode & S_IRUSR) ? 'r' : '-';
            perm[2] = (statBuf.st_mode & S_IWUSR) ? 'w' : '-';
            perm[3] = (statBuf.st_mode & S_IXUSR) ? 'x' : '-';
            perm[4] = (statBuf.st_mode & S_IRGRP) ? 'r' : '-';
            perm[5] = (statBuf.st_mode & S_IWGRP) ? 'w' : '-';
            perm[6] = (statBuf.st_mode & S_IXGRP) ? 'x' : '-';
            perm[7] = (statBuf.st_mode & S_IROTH) ? 'r' : '-';
            perm[8] = (statBuf.st_mode & S_IWOTH) ? 'w' : '-';
            perm[9] = (statBuf.st_mode & S_IXOTH) ? 'x' : '-';

            strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M", localtime(&statBuf.st_mtime));

            char line[512];
            snprintf(line, sizeof(line), "%s 1 owner group %8lld %s %s\r\n",
                     perm, (long long)statBuf.st_size, timeBuf, entry->d_name);
            listing += line;
        }

        closedir(dir);

        if (utf8)
            listing = convertEncoding(listing, ServerInfo::encoding, "UTF-8");

        size_t totalSent = 0;
        bool sendError = false;
        while (totalSent < listing.length())
        {
            ssize_t sent = send(dataSocket, listing.c_str() + totalSent, listing.length() - totalSent, 0);

            if (sent < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    co_await std::suspend_always{};
                    if (error)
                        co_return;
                    continue;
                }
                sendError = true;
                break;
            }
            totalSent += sent;
        }

        if (sendError)
            sWaitSend.append("426 Connection error during transfer.\r\n");
        else
            sWaitSend.append("226 Directory send OK.\r\n");

        CloseDataConnection();
        co_return;
    }

    Generator<START_FLAG::START_FLAG_NOSUSPEND> FTPServer::HandleUpload(const std::string &filename)
    {
        size_t index = filename.find_last_of('/');
        std::string tFilenames;
        if (index != std::string::npos)
        {
            tFilenames = filename.substr(index + 1);
        }
        else
        {
            tFilenames = filename;
        }
        std::string filePath = currentDir + "/" + tFilenames;
        sWaitSend.append("150 Opening data connection for ").append(tFilenames).append(".\r\n");

        while (!Send())
        {
            co_await std::suspend_always{};
            if (error)
                co_return;
        }

        if (!EstablishDataConnection())
        {
            sWaitSend.append("425 Can't open data connection.\r\n");
            co_return;
        }

        int fileHandle = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fileHandle < 0)
        {
            sWaitSend.append("550 Failed to create file.\r\n");
            CloseDataConnection();
            co_return;
        }

        char buffer[8192];
        ssize_t bytesReceived;
        bool timeoutOccurred = false;
        while (true)
        {
            bytesReceived = recv(dataSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0)
            {
                ssize_t bytesWritten = 0;

                while (bytesWritten < bytesReceived)
                {
                    ssize_t result = write(fileHandle, buffer + bytesWritten, (size_t)(bytesReceived - bytesWritten));
                    if (result > 0)
                    {
                        bytesWritten += result;
                    }
                    else if (result < 0)
                    {
                        sWaitSend.append("552 Storage allocation exceeded.\r\n");
                        goto close_;
                    }
                }
            }
            else if (bytesReceived == 0)
            {
                break;
            }
            else if (bytesReceived < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    co_await std::suspend_always{};
                    if (error)
                    {
                        close(fileHandle);
                        co_return;
                    }
                    continue;
                }
                else
                {
                    sWaitSend.append("426 Connection error during transfer.\r\n");
                    goto close_;
                }
            }
        }

        sWaitSend.append("226 Transfer complete.\r\n");

    close_:
        close(fileHandle);
        CloseDataConnection();
        co_return;
    }

    Generator<START_FLAG::START_FLAG_NOSUSPEND> FTPServer::HandleDownload(const std::string &filename)
    {
        std::string filePath = currentDir + "/" + filename;

        struct stat statbuf;
        if (stat(filePath.c_str(), &statbuf) || !S_ISREG(statbuf.st_mode))
        {
            sWaitSend.append("550 File not found.\r\n");
            co_return;
        }

        sWaitSend.append("150 Opening data connection for ").append(filename).append(".\r\n");

        while (!Send())
        {
            co_await std::suspend_always{};
            if (error)
                co_return;
        }

        if (!EstablishDataConnection())
        {
            sWaitSend.append("425 Can't open data connection.\r\n");
            co_return;
        }

        int fileHandle = open(filePath.c_str(), O_RDONLY);
        if (fileHandle < 0)
        {
            sWaitSend.append("550 Failed to open file.\r\n");
            CloseDataConnection();
            co_return;
        }

        char buffer[8192];
        ssize_t bytesRead;
        while ((bytesRead = read(fileHandle, buffer, sizeof(buffer))) > 0)
        {
            size_t bytesSent = 0;

            while (bytesSent < bytesRead)
            {
                ssize_t result = send(dataSocket, buffer + bytesSent, (size_t)(bytesRead - bytesSent), 0);
                if (result > 0)
                {
                    bytesSent += result;
                }
                else if (result < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        co_await std::suspend_always{};
                        if (error)
                        {
                            close(fileHandle);
                            co_return;
                        }
                        continue;
                    }
                    else
                    {
                        sWaitSend.append("426 Connection error during transfer.\r\n");
                        goto close_;
                    }
                }
            }
        }

        sWaitSend.append("226 Transfer complete.\r\n");

    close_:
        close(fileHandle);
        CloseDataConnection();
        co_return;
    }

    bool FTPServer::ProcessCommand(std::string &cmd, std::string &param)
    {
        cmd = ToUpperCase(cmd);

        if (utf8)
            param = convertEncoding(param, "UTF-8", ServerInfo::encoding);

        HSLL_LOGINFO(LOG_LEVEL_INFO, info.ip, ":", info.port, " Command: [", cmd, "] Param: [", param, "]");

        if (cmd == "USER")
        {
            user = param;
            sWaitSend.append("331 User name okay, need password.\r\n");
            return true;
        }
        else if (cmd == "PASS")
        {
            if (user == "anonymous")
            {
                if (ServerInfo::anonymous)
                {
                    certified = true;
                    sWaitSend.append("230 User logged in.\r\n");
                }
                else
                {
                    sWaitSend.append("530 Anonymous access not allowed.\r\n");
                }
                return true;
            }

            if (ServerInfo::users.find({user, param}) != ServerInfo::users.end())
            {
                certified = true;
                sWaitSend.append("230 User logged in.\r\n");
            }
            else
            {
                sWaitSend.append("530 Login incorrect.\r\n");
            }
            return true;
        }
        else if (cmd == "OPTS")
        {
            if (ServerInfo::utf8)
            {
                if (param == "utf8 on" || param == "UTF8 ON")
                {
                    utf8 = true;
                    sWaitSend.append("200 UTF-8 mode enabled.\r\n");
                }
                else if (param == "utf8 off" || param == "UTF8 OFF")
                {
                    utf8 = false;
                    sWaitSend.append("200 UTF-8 mode disabled.\r\n");
                }
                else
                {
                    sWaitSend.append("501 Option not supported.\r\n");
                }
                return true;
            }
            sWaitSend.append("501 Option not supported.\r\n");
            return true;
        }

        if (!certified)
        {
            sWaitSend.append("550 Permission denied.\r\n");
            return true;
        }

        if (param.empty())
        {
            if (cmd == "PWD")
            {
                std::string convert = "257 \"" + currentDir + "\"\r\n";
                if (utf8)
                    convert = convertEncoding(convert, ServerInfo::encoding, "UTF-8");
                sWaitSend.append(convert);
            }
            else if (cmd == "SYST")
            {
                sWaitSend.append("215 UNIX Type: L8\r\n");
            }
            else if (cmd == "FEAT")
            {
                sWaitSend.append("211-Features:\r\n PASV\r\n SIZE\r\n");
                if (ServerInfo::utf8)
                    sWaitParse.append(" UTF8\r\n OPTS UTF8\r\n");
                sWaitSend.append(" 211 End\r\n");
            }
            else if (cmd == "QUIT")
            {
                sWaitSend.append("221 Goodbye\r\n");
            }
            else if (cmd == "NOOP")
            {
                sWaitSend.append("200 NOOP ok\r\n");
            }
            else if (cmd == "TYPE")
            {
                sWaitSend.append("200 Type set to I\r\n");
            }
            else if (cmd == "PASV")
            {
                HandlePASV();
            }
            else if (cmd == "LIST" || cmd == "NLST")
            {
                task = HandleList();
                if (!task.hasDone())
                    return false;
                task.Destroy();
            }
            else
            {
                sWaitSend.append("501 Syntax error\r\n");
            }
        }
        else
        {
            if (cmd == "CWD" || cmd == "XCWD")
            {
                std::string targetDir = (param[0] == '/') ? param : currentDir + "/" + param;
                DIR *dir = opendir(targetDir.c_str());
                if (dir != nullptr)
                {
                    closedir(dir);
                    currentDir = targetDir;
                    sWaitSend.append("250 Directory changed to " + targetDir + ".\r\n");
                }
                else
                {
                    sWaitSend.append("550 Failed to change directory. Directory does not exist or is not accessible.\r\n");
                }
            }
            else if (cmd == "RMD")
            {
                std::string dirPath = currentDir + "/" + param;
                if (rmdir(dirPath.c_str()) == 0)
                {
                    sWaitSend.append("250 Directory removed.\r\n");
                }
                else
                {
                    sWaitSend.append("550 Remove failed.\r\n");
                }
            }
            else if (cmd == "TYPE")
            {
                if (param == "A" || param == "I")
                {
                    sWaitSend.append("200 Type set to ").append(param).append("\r\n");
                }
                else
                {
                    sWaitSend.append("504 Invalid type.\r\n");
                }
            }
            else if (cmd == "PORT")
            {
                HandlePORT(param);
            }
            else if (cmd == "SIZE")
            {
                std::string filePath = currentDir + "/" + param;
                struct stat statbuf;
                if (stat(filePath.c_str(), &statbuf) == 0)
                {
                    sWaitSend.append("213 ").append(std::to_string((long long)statbuf.st_size)).append("\r\n");
                }
                else
                {
                    sWaitSend.append("550 File not found.\r\n");
                }
            }
            else if (cmd == "RNFR")
            {
                std::string filePath = currentDir + "/" + param;
                struct stat statbuf;
                if (stat(filePath.c_str(), &statbuf) == 0)
                {
                    renameFromPath = filePath;
                    sWaitSend.append("350 Ready for RNTO.\r\n");
                }
                else
                {
                    sWaitSend.append("550 File not found.\r\n");
                }
            }
            else if (cmd == "RNTO")
            {
                if (renameFromPath.empty())
                {
                    sWaitSend.append("503 RNFR required.\r\n");
                }
                else
                {
                    std::string filePath = currentDir + "/" + param;
                    if (rename(renameFromPath.c_str(), filePath.c_str()) == 0)
                    {
                        sWaitSend.append("250 Rename ok.\r\n");
                    }
                    else
                    {
                        sWaitSend.append("550 Rename failed.\r\n");
                    }
                    renameFromPath.clear();
                }
            }
            else if (cmd == "DELE")
            {
                std::string filePath = currentDir + "/" + param;
                if (remove(filePath.c_str()) == 0)
                {
                    sWaitSend.append("250 File deleted.\r\n");
                }
                else
                {
                    sWaitSend.append("550 Delete failed.\r\n");
                }
            }
            else if (cmd == "RETR")
            {
                task = HandleDownload(param);
                if (!task.hasDone())
                    return false;
                task.Destroy();
            }
            else if (cmd == "STOR")
            {
                task = HandleUpload(param);
                if (!task.hasDone())
                    return false;
                task.Destroy();
            }
            else if (cmd == "MKD" || cmd == "XMKD")
            {
                std::string dirPath = currentDir + "/" + param;
                if (mkdir(dirPath.c_str(), 0755) == 0)
                {
                    sWaitSend.append("257 \"").append(param).append("\" created.\r\n");
                }
                else
                {
                    sWaitSend.append((errno == EEXIST) ? "550 Exists\r\n" : "550 Create failed\r\n");
                }
            }
            else
            {
                sWaitSend.append("500 Command error.\r\n");
            }
        }
        return true;
    }

    bool FTPServer::DealTask()
    {
        if (task.HandleInvalid())
        {
            task.Resume();

            if (task.hasDone())
            {
                task.Destroy();
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    void FTPServer::DealRead()
    {
        Read();
        if (!DealTask())
            return;
        if (!Parse())
            error = true;
        Send_And_EnableWR();
    }

    void FTPServer::DealWrite()
    {
        if (!DealTask())
            return;
        Send_And_EnableWR();
    }

    void FTPServer::DealAccept()
    {
        sWaitSend.append("220 Welcome\r\n");
        Send_And_EnableWR();
    }

    void FTPServer::Send_And_EnableWR()
    {
        Send();
        enableFree = true;
        evb.EnableWR();
    }

    void FTPServer::EnableRW()
    {
        enableFree = true;
        evb.EnableWR();
    }

    void FTPServer::DisableRW()
    {
        enableFree = false;
        evb.DisableWR();
    }

    bool FTPServer::CheakFree()
    {
        return enableFree;
    }

    bool FTPServer::CheakError()
    {
        return error;
    }

    void FTPServer::Read()
    {
        char buf[1024];
        while (true)
        {
            int tSize = evb.Read(buf, 1024);
            if (tSize <= 0)
                break;
            sWaitParse.append(buf, (size_t)tSize);
            if (tSize < 1024)
                break;
        }
    }

    bool FTPServer::Send()
    {
        if (sWaitSend.empty())
            return true;

        if (evb.Write(sWaitSend.c_str(), sWaitSend.length()) == 0)
        {
            sWaitSend.clear();
            return true;
        }
        return false;
    }

    bool FTPServer::Parse()
    {
        size_t start = 0;
        size_t end = 0;
        bool parseStop = false;
        while (true)
        {
            end = sWaitParse.find("\r\n", start);
            if (end == std::string::npos || parseStop)
                break;

            std::string line = sWaitParse.substr(start, end - start);
            size_t spacePos = line.find(' ');
            std::string command = (spacePos != std::string::npos) ? line.substr(0, spacePos) : line;
            std::string param = (spacePos != std::string::npos) ? line.substr(spacePos + 1) : "";
            parseStop = !ProcessCommand(command, param);
            start = end + 2;
        }

        if (start < sWaitParse.length())
            sWaitParse = sWaitParse.substr(start);
        else
            sWaitParse.clear();

        return (sWaitParse.size() <= 1024);
    }

    FTPServer::FTPServer(EVBuffer evb, ConnectionInfo info) : evb(evb),
                                                              info(info),
                                                              enableFree(true),
                                                              certified(false),
                                                              utf8(false),
                                                              error(false),
                                                              dataSocket(-1),
                                                              pasvSocket(-1),
                                                              dataMode(DATA_MODE_NONE),
                                                              currentDir(ServerInfo::dir)
    {
    }

    FTPServer::~FTPServer()
    {
        error = true;
        if (task.HandleInvalid())
        {
            task.Resume();
            task.Destroy();
        }
        CloseDataConnection();
    }
}