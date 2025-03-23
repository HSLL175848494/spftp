
#include "Eventcplus.h"
#include <thread>
#include <cstring>

namespace HSLL
{
    ReadProc EVSocket::rp = nullptr;
    WriteProc EVSocket::wp = nullptr;
    ConnectProc EVSocket::cnp = nullptr;
    CloseProc EVSocket::csp = nullptr;
    event_base *EVSocket::base = nullptr;
    EVSocket *EVSocket::instance = nullptr;
    std::map<bufferevent *, std::pair<ConnectionInfo, void *>> EVSocket::cnts;

    const char *const EVSocket::errorStrs[] = {
        "No error",
        "Failed to construct EVSocket",
        "evconnlistener_new_bind() failed",
        "event_base_dispatch() failed",
        "Parameters cannot be null",
        "event_base_loopbreak() failed",
        "Incorrect call sequence, please call in order: SetService()->Listen()->SetSignalExit()->EventLoop()",
        "evsignal_new() failed",
        "Signal event event_add() failed"};

    int EVBuffer::Read(void *buf, unsigned int size)
    {
        return evbuffer_remove(input, buf, size);
    }

    int EVBuffer::Write(const void *buf, unsigned int size)
    {
        return evbuffer_add(output, buf, size);
    }

    int EVBuffer::EnableWR()
    {
        return bufferevent_enable(bev, EV_READ | EV_WRITE);
    }

    int EVBuffer::DisableWR()
    {
        return bufferevent_disable(bev, EV_READ | EV_WRITE);
    }

    EVBuffer::EVBuffer(bufferevent *bev, event_base *base) : bev(bev), base(base)
    {
        input = bufferevent_get_input(bev);
        output = bufferevent_get_output(bev);
    }

    void EVSocket::GetHostInfo(sockaddr *address, ConnectionInfo *info)
    {
        sockaddr_in *addr_in = (sockaddr_in *)(address);
        inet_ntop(AF_INET, &(addr_in->sin_addr), info->ip, INET_ADDRSTRLEN);
        info->port = ntohs(addr_in->sin_port);
    }

    void EVSocket::Callback_Accept(evconnlistener *listener, evutil_socket_t fd,
                                   sockaddr *address, int socklen, void *ctx)
    {
        event_base *base = evconnlistener_get_base(listener);
        bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
        HSLL_EXP_FUNC_LOGINFO(bev == nullptr, return, LOG_LEVEL_ERROR,
                              "bufferevent_socket_new() failed: ", HSLL_SOCKET_GET_ERROR);

        ConnectionInfo info{};
        GetHostInfo(address, &info);

        HSLL_EXP_FUNC_LOGINFO(bufferevent_enable(bev, EV_READ | EV_WRITE) != 0, return,
                              LOG_LEVEL_ERROR, "bufferevent_enable() failed: ", HSLL_SOCKET_GET_ERROR)

        void *ctx2 = EVSocket::cnp(EVBuffer(bev, base), info);
        EVSocket::cnts.insert({bev, {info, ctx2}});
        bufferevent_setcb(bev, Callback_Read, Callback_Write, Callback_Event, ctx2);
        HSLL_LOGINFO(LOG_LEVEL_INFO, "Connection accepted: ", info.ip, ":", info.port)
    }

#define HSLL_SOCKET_CLOSE                                                           \
    EVSocket::csp(ctx);                                                             \
    bufferevent_free(bev);                                                          \
    ConnectionInfo *info = &EVSocket::cnts.find(bev)->second.first;                 \
    HSLL_LOGINFO(LOG_LEVEL_INFO, "Connection closed: ", info->ip, ":", info->port); \
    EVSocket::cnts.erase(bev);

    void EVSocket::Callback_Read(bufferevent *bev, void *ctx)
    {
        if (EVSocket::rp(ctx) == false)
        {
            HSLL_SOCKET_CLOSE
        }
    }

    void EVSocket::Callback_Write(bufferevent *bev, void *ctx)
    {
        if (EVSocket::wp(ctx) == false)
        {
            HSLL_SOCKET_CLOSE
        }
    }

    void EVSocket::Callback_Event(bufferevent *bev, short events, void *ctx)
    {
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
        {
            HSLL_SOCKET_CLOSE
        }
    }

    void EVSocket::Callback_Error(evconnlistener *listener, void *ctx)
    {
        event_base *base = evconnlistener_get_base(listener);
        HSLL_LOGINFO(LOG_LEVEL_ERROR, "Socket error: ",
                     evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()))
        HSLL_EXP_LOGINFO(event_base_loopexit(base, nullptr) != 0, LOG_LEVEL_ERROR,
                         "event_base_loopexit() failed: ", HSLL_SOCKET_GET_ERROR)
    }

    void EVSocket::Callback_Signal(evutil_socket_t sig, short events, void *ctx)
    {
        HSLL_LOGINFO(LOG_LEVEL_CRUCIAL, "");

        for (auto i : EVSocket::cnts)
        {
            EVSocket::csp(i.second.second);
            bufferevent_free(i.first);
            HSLL_LOGINFO(LOG_LEVEL_INFO, "Connection closed: ", i.second.first.ip, ":", i.second.first.port);
        }

        HSLL_LOGINFO(LOG_LEVEL_INFO, "Received signal ", sig, ", preparing to exit event loop");
        HSLL_EXP_LOGINFO(event_base_loopbreak(EVSocket::base) != 0, LOG_LEVEL_ERROR,
                         "event_base_loopbreak() failed: ", HSLL_SOCKET_GET_ERROR);
    }

    EVSocket::EVSocket(unsigned short port, const char *ip)
        : port(port), status(0), evExit(nullptr), listener(nullptr)
    {
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(ip);
        sin.sin_port = htons(port);

        evthread_use_pthreads();
        if ((base = event_base_new()) == nullptr)
            HSLL_SOCKET_PRINT_DEBUG(1);
    }

    EVSocket *EVSocket::Construct(unsigned short port, const char *ip)
    {
        if (instance == nullptr)
            return instance = new EVSocket(port, ip);
        return nullptr;
    }

    unsigned int EVSocket::SetService(ConnectProc cp, CloseProc dcp, ReadProc rp, WriteProc wp)
    {
        if((status % 10)>0)
        return 0;

        HSLL_SOCKET_ERROR_RET(cp == nullptr || dcp == nullptr ||
                                  rp == nullptr || wp == nullptr,
                              4);
        cbAccept = Callback_Accept;
        this->cnp = cp;
        this->csp = dcp;
        this->rp = rp;
        this->wp = wp;
        status += 1;
        return 0;
    }

    unsigned int EVSocket::Listen()
    {
        if((status % 10)>1)
        return 0;

        HSLL_SOCKET_ERROR_RET((status % 10) != 1, 6)
        listener = evconnlistener_new_bind(
            base, cbAccept, nullptr, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
            (sockaddr *)&sin, sizeof(sin));
        HSLL_SOCKET_ERROR_RET(listener == nullptr, 2)

        evconnlistener_set_error_cb(listener, Callback_Error);
        HSLL_LOGINFO(LOG_LEVEL_INFO, "Listening on port: ", port);
        status += 1;
        return 0;
    }

    unsigned int EVSocket::SetSignalExit(int sg)
    {
        if(status>10)
        return 0;

        HSLL_SOCKET_ERROR_RET(base == nullptr, 6);
        evExit = evsignal_new(base, sg, Callback_Signal, nullptr);
        HSLL_SOCKET_ERROR_RET(evExit == nullptr, 7);
        HSLL_SOCKET_ERROR_RET(event_add(evExit, nullptr) != 0, 8);
        HSLL_LOGINFO(LOG_LEVEL_INFO, "Signal handler set, program can be terminated with signal: ", sg);
        status += 10;
        return 0;
    }

    unsigned int EVSocket::EventLoop()
    {
        HSLL_SOCKET_ERROR_RET((status % 10) != 2, 6)
        if(status<10)
        HSLL_LOGINFO(LOG_LEVEL_CRUCIAL, "Exit signal not set: When no signal is set, Release() "
        "will force all connections to close. Make sure that the connection is no longer referenced at this point")
        HSLL_LOGINFO(LOG_LEVEL_INFO, "Entering event loop")
        int ret = event_base_dispatch(base);
        HSLL_SOCKET_ERROR_RET(ret != 0 && ret != 1, 3);
        return 0;
    }

    const char *EVSocket::getLastError(int code)
    {
        return errorStrs[code];
    }

    void EVSocket::Release()
    {
        if (instance)
        {
            if (instance->evExit)
                event_free(instance->evExit);

            if (instance->listener)
                evconnlistener_free(instance->listener);

            if (base)
                event_base_free(base);
            delete instance;
        }
    }
}