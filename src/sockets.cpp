//
// Copyright (C) 2001-2020 Maciej Sobczak
//
// This file declares wrappers that can be used
// as IOStream-compatible TCP/IP or Unix sockets.
//
// On Windows the program that uses this utility
// should be linked with Ws2_32.lib.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file Boost_Software_License_1_0.txt
// or copy at http://www.opensource.org/licenses/bsl1.0.html)
//

#include "sockets.h"

#include <sstream>

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <cerrno>
#include <netdb.h>
#include <arpa/inet.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) ::close(s)
#endif

#include <stdio.h>

socket_runtime_error::socket_runtime_error(const std::string & what)
    : runtime_error(what)
{
#ifdef WIN32
    errnum_ = ::WSAGetLastError();
#else
    errnum_ = errno;
#endif
}

const char * socket_runtime_error::what() const throw()
{
    std::ostringstream ss;
    ss << runtime_error::what();
    ss << " error number: " << errnum_;
    msg_ = ss.str();
    return msg_.c_str();
}

base_socket_wrapper::~base_socket_wrapper()
{
    if (sockstate_ != CLOSED)
    {
        closesocket(sock_);
    }
}

void base_socket_wrapper::write(const void * buf, std::size_t len)
{
    if (sockstate_ != CONNECTED && sockstate_ != ACCEPTED)
    {
        throw socket_logic_error("socket not connected");
    }

    int written;
    while (len != 0)
    {
        if ((written = send(sock_, (const char *)buf, (int)len, 0))
            == SOCKET_ERROR)
        {
            throw socket_runtime_error("write failed");
        }

        len -= written;
        buf = (const char *)buf + written;
    }
}

std::size_t base_socket_wrapper::read(void * buf, size_t len)
{
    if (sockstate_ != CONNECTED && sockstate_ != ACCEPTED)
    {
        throw socket_logic_error("socket not connected");
    }

    int readn = recv(sock_, (char *)buf, (int)len, 0);
    if (readn == SOCKET_ERROR)
    {
        throw socket_runtime_error("read failed");
    }

    return (std::size_t)readn;
}

void base_socket_wrapper::close()
{
    if (sockstate_ != CLOSED)
    {
        if (closesocket(sock_) == SOCKET_ERROR)
        {
            throw socket_runtime_error("close failed");
        }
        sockstate_ = CLOSED;
    }
}

tcp_socket_wrapper::tcp_socket_wrapper(
    const tcp_socket_wrapper::tcp_accepted_socket & as)
    : base_socket_wrapper(as), sockaddress_(as.addr_)
{
}

void tcp_socket_wrapper::listen(int port, int backlog)
{
    if (sockstate_ != CLOSED)
    {
        throw socket_logic_error("socket not in CLOSED state");
    }

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET)
    {
        throw socket_runtime_error("socket failed");
    }

#ifdef _WIN32
    char option_value = 1;
#else
    int option_value = 1;
#endif
    if (::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
        &option_value, sizeof(option_value)) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("setsockopt failed");
    }
    
    sockaddr_in local;

    memset(&local, 0, sizeof(local));

    local.sin_family = AF_INET;
    local.sin_port = htons((u_short)port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(sock_, (sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("bind failed");
    }

    if (::listen(sock_, backlog) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("listen failed");
    }

    memset(&sockaddress_, 0, sizeof(sockaddress_));
    sockstate_ = LISTENING;
}

tcp_socket_wrapper::tcp_accepted_socket tcp_socket_wrapper::accept()
{
    if (sockstate_ != LISTENING)
    {
        throw socket_logic_error("socket not listening");
    }

    sockaddr_in from;
    socklen_t len = sizeof(from);

    memset(&from, 0, len);

    socket_type newsocket = ::accept(sock_, (sockaddr *)&from, &len);
    if (newsocket == INVALID_SOCKET)
    {
        throw socket_runtime_error("accept failed");
    }

#ifdef _WIN32
    char option_value = 1;
#else
    int option_value = 1;
#endif
    if (::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY,
        &option_value, sizeof(option_value)) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("setsockopt failed");
    }

    return tcp_accepted_socket(newsocket, from);
}

void tcp_socket_wrapper::connect(const std::string & address, int port)
{
    if (sockstate_ != CLOSED)
    {
        throw socket_logic_error("socket not in CLOSED state");
    }

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET)
    {
        throw socket_runtime_error("socket failed");
    }

    hostent * hp;

    unsigned long addr = inet_addr(address.c_str());
    if (addr != INADDR_NONE)
    {
        hp = gethostbyaddr((const char *)&addr, 4, AF_INET);
    }
    else
    {
        hp = gethostbyname(address.c_str());
    }

    if (hp == NULL)
    {
        closesocket(sock_);
        throw socket_runtime_error("cannot resolve address");
    }

    if (hp->h_addrtype != AF_INET)
    {
        closesocket(sock_);
        throw socket_runtime_error
            ("address resolved with TCP incompatible type");
    }

    memset(&sockaddress_, 0, sizeof(sockaddress_));
    memcpy(&(sockaddress_.sin_addr), hp->h_addr_list[0], hp->h_length);
    sockaddress_.sin_family = AF_INET;
    sockaddress_.sin_port = htons((u_short)port);

    if (::connect(sock_, (sockaddr *)&sockaddress_, sizeof(sockaddress_))
        == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("connect failed");
    }

#ifdef _WIN32
    char option_value = 1;
#else
    int option_value = 1;
#endif
    if (::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY,
        &option_value, sizeof(option_value)) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("setsockopt failed");
    }

    sockstate_ = CONNECTED;
}

std::string tcp_socket_wrapper::address() const
{
    if (sockstate_ != CONNECTED && sockstate_ != ACCEPTED)
    {
        throw socket_logic_error("socket not connected");
    }

    return inet_ntoa(sockaddress_.sin_addr);
}

int tcp_socket_wrapper::port() const
{
    if (sockstate_ != CONNECTED && sockstate_ != ACCEPTED)
    {
        throw socket_logic_error("socket not connected");
    }

    return ntohs(sockaddress_.sin_port);
}

#ifndef WIN32

unix_socket_wrapper::unix_socket_wrapper(
    const unix_socket_wrapper::unix_accepted_socket & as)
    : base_socket_wrapper(as), sockaddress_(as.addr_)
{
}

void unix_socket_wrapper::listen(const std::string & path, int backlog)
{
    if (sockstate_ != CLOSED)
    {
        throw socket_logic_error("socket not in CLOSED state");
    }

    ::unlink(path.c_str());

    sock_ = ::socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET)
    {
        throw socket_runtime_error("socket failed");
    }

    sockaddr_un local;

    memset(&local, 0, sizeof(local));

    local.sun_family = AF_LOCAL;
    ::strcpy(local.sun_path, path.c_str());

    if (::bind(sock_, (sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("bind failed");
    }

    if (::listen(sock_, backlog) == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("listen failed");
    }

    memset(&sockaddress_, 0, sizeof(sockaddress_));
    sockstate_ = LISTENING;
}

unix_socket_wrapper::unix_accepted_socket unix_socket_wrapper::accept()
{
    if (sockstate_ != LISTENING)
    {
        throw socket_logic_error("socket not listening");
    }

    sockaddr_un from;
    socklen_t len = sizeof(from);

    memset(&from, 0, len);

    socket_type newsocket = ::accept(sock_, (sockaddr *)&from, &len);
    if (newsocket == INVALID_SOCKET)
    {
        throw socket_runtime_error("accept failed");
    }

    return unix_accepted_socket(newsocket, from);
}

void unix_socket_wrapper::connect(const std::string & path)
{
    if (sockstate_ != CLOSED)
    {
        throw socket_logic_error("socket not in CLOSED state");
    }

    sock_ = ::socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET)
    {
        throw socket_runtime_error("socket failed");
    }

    memset(&sockaddress_, 0, sizeof(sockaddress_));
    sockaddress_.sun_family = AF_LOCAL;
    ::strcpy(sockaddress_.sun_path, path.c_str());

    if (::connect(sock_, (sockaddr *)&sockaddress_, sizeof(sockaddress_))
        == SOCKET_ERROR)
    {
        closesocket(sock_);
        throw socket_runtime_error("connect failed");
    }

    sockstate_ = CONNECTED;
}

std::string unix_socket_wrapper::path() const
{
    if (sockstate_ != CONNECTED && sockstate_ != ACCEPTED)
    {
        throw socket_logic_error("socket not connected");
    }

    return sockaddress_.sun_path;
}

#endif // WIN32
