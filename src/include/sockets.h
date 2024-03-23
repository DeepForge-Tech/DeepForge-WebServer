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

#ifndef SOCKETS_H_INCLUDED
#define SOCKETS_H_INCLUDED

#ifdef _WIN32
#include <Winsock2.h>
typedef int socklen_t;
#else // POSIX
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/un.h>
#endif

#include <string>
#include <streambuf>
#include <iostream>
#include <ostream>
#include <stdexcept>

#include <stdio.h>

// exception class which designates errors from socket functions
class socket_runtime_error : public std::runtime_error
{
public:
    explicit socket_runtime_error(const std::string & what);
    virtual ~socket_runtime_error() throw () { }

    virtual const char * what() const throw();
    int errornumber() const throw() { return errnum_; }

private:
    // this will serve as a message returned from what()
    mutable std::string msg_;
    int errnum_;
};

// exception class which designates logic (programming) errors with sockets
class socket_logic_error : public std::logic_error
{
public:
    explicit socket_logic_error(const std::string & what)
        : std::logic_error(what)
    {
    }
};

// this class is a base class for both TCP and Unix sockets
class base_socket_wrapper
{
public:
#ifdef WIN32
    // on Windows, socket is represented by the opaque handler
    typedef SOCKET socket_type;
#else
    // on Linux, socket is just a descriptor number
    typedef int socket_type;
#endif

    enum sockstate_type { CLOSED, LISTENING, ACCEPTED, CONNECTED };

    base_socket_wrapper() : sockstate_(CLOSED) {}
    virtual ~base_socket_wrapper();

    // general methods

    // get the current state of the socket wrapper
    sockstate_type state() const { return sockstate_; }

    // write data to the socket
    void write(const void * buf, size_t len);

    // read data from the socket
    // returns the number of bytes read
    size_t read(void * buf, size_t len);

    void close();

protected:

    // proxy helper for syntax:
    // sock s2(s1.accept());
    template <class address_type, class socket_wrapper>
    class accepted_socket
    {
    public:
        // only copy constructor provided for the proxy
        // so that the socket_wrapper::accept can
        // successfully return by value
        accepted_socket(const accepted_socket & a)
            : sock_(a.sock_), addr_(a.addr_)
        {
        }

        accepted_socket(socket_type s, address_type a)
            : sock_(s), addr_(a)
        {
        }

        socket_type sock_;
        address_type addr_;
    
    private:
        // assignment not provided
        void operator=(const accepted_socket &);
    };

    template <class address_type, class socket_wrapper>
    base_socket_wrapper(const accepted_socket<address_type, socket_wrapper> & as)
        : sock_(as.sock_), sockstate_(ACCEPTED)
    {
    }

    socket_type sock_;
    sockstate_type sockstate_;

private:
    // not provided
    base_socket_wrapper(const base_socket_wrapper &);
    void operator=(const base_socket_wrapper &);
};

// this class serves as a socket wrapper
class tcp_socket_wrapper : public base_socket_wrapper
{
    typedef base_socket_wrapper::accepted_socket<sockaddr_in, tcp_socket_wrapper>
    tcp_accepted_socket;

public:

    tcp_socket_wrapper() {}

    // this is provided for syntax
    // tcp_socket_wrapper s2(s2.accept());
    tcp_socket_wrapper(const tcp_accepted_socket & as);

    // server methods

    // binds and listens on a given port number
    void listen(int port, int backlog = 100);
    
    // accepts the new connection
    // it requires the earlier call to listen
    tcp_accepted_socket accept();

    // client methods

    // creates the new connection
    void connect(const std::string & address, int port);

    // general methods

    // get the network address and port number of the socket
    std::string address() const;
    int port() const;

private:
    // not for use
    tcp_socket_wrapper(const tcp_socket_wrapper &);
    void operator=(const tcp_socket_wrapper &);

    sockaddr_in sockaddress_;
};

#ifndef WIN32

// this class serves as a Unix socket wrapper
// (obviously, available only on POSIX systems)
class unix_socket_wrapper : public base_socket_wrapper
{
    typedef base_socket_wrapper::accepted_socket<sockaddr_un, unix_socket_wrapper>
    unix_accepted_socket;

public:

    unix_socket_wrapper() {}

    // this is provided for syntax
    // unix_socket_wrapper s2(s2.accept());
    unix_socket_wrapper(const unix_accepted_socket & as);

    // server methods

    // binds and listens on a given socket path
    void listen(const std::string & path, int backlog = 100);
    
    // accepts the new connection
    // it requires the earlier call to listen
    unix_accepted_socket accept();

    // client methods

    // creates the new connection
    void connect(const std::string & path);

    // general methods

    // get the socket path
    std::string path() const;

private:
    // not for use
    unix_socket_wrapper(const unix_socket_wrapper &);
    void operator=(const unix_socket_wrapper &);

    sockaddr_un sockaddress_;
};

#endif // WIN32

// this class is supposed to serve as a stream buffer associated with a socket
template <class socket_wrapper,
          class charT, class traits = std::char_traits<charT> >
class socket_stream_buffer : public std::basic_streambuf<charT, traits>
{
    typedef std::basic_streambuf<charT, traits> sbuftype;
    typedef typename sbuftype::int_type         int_type;
    typedef charT                               char_type;

public:

    // the buffer will take ownership of the socket
    // (ie. it will close it in the destructor) if takeowner == true
    explicit socket_stream_buffer(socket_wrapper & sock,
        bool takeowner = false, std::streamsize bufsize = 512)
        : rsocket_(sock), ownsocket_(takeowner),
          inbuf_(NULL), outbuf_(NULL), bufsize_(bufsize),
          remained_(0), ownbuffers_(false)
    {
    }

    ~socket_stream_buffer()
    {
        try
        {
            if (rsocket_.state() == socket_wrapper::CONNECTED ||
                rsocket_.state() == socket_wrapper::ACCEPTED)
            {
                _flush();
            }

            if (ownsocket_ == true)
            {
                rsocket_.close();
            }
        }
        catch (...)
        {
            // we don't want exceptions to fly out of here
            // and there is not much we can do with errors
            // in this context anyway
        }

        if (ownbuffers_)
        {
            delete [] inbuf_;
            delete [] outbuf_;
        }
    }

protected:
    sbuftype * setbuf(char_type * s, std::streamsize n)
    {
        if (this->gptr() == NULL)
        {
            sbuftype::setg(s, s + n, s + n);
            sbuftype::setp(s, s + n);
            inbuf_ = s;
            outbuf_ = s;
            bufsize_ = n;
            ownbuffers_ = false;
        }

        return this;
    }

    void _flush()
    {
        rsocket_.write(outbuf_,
            (this->pptr() - outbuf_) * sizeof(char_type));
    }

    int_type overflow(int_type c = traits::eof())
    {
        // this method is supposed to flush the put area of the buffer
        // to the I/O device

        // if the buffer was not already allocated nor set by user,
        // do it just now
        if (this->pptr() == NULL)
        {
            outbuf_ = new char_type[bufsize_];
            ownbuffers_ = true;
        }
        else
        {
            _flush();
        }

        sbuftype::setp(outbuf_, outbuf_ + bufsize_);

        if (c != traits::eof())
        {
            sbuftype::sputc(traits::to_char_type(c));
        }

        return 0;
    }

    int sync()
    {
        // just flush the put area
        _flush();
        sbuftype::setp(outbuf_, outbuf_ + bufsize_);
        return 0;
    }

    int_type underflow()
    {
        // this method is supposed to read some bytes from the I/O device

        // if the buffer was not already allocated nor set by user,
        // do it just now
        if (this->gptr() == NULL)
        {
            inbuf_ = new char_type[bufsize_];
            ownbuffers_ = true;
        }

        if (remained_ != 0)
        {
            inbuf_[0] = remainedchar_;
        }

        size_t readn = rsocket_.read(static_cast<char *>(inbuf_) + remained_,
            bufsize_ * sizeof(char_type) - remained_);

        // if (readn == 0 && remained_ != 0)
        // error - there is not enough bytes for completing
        // the last character before the end of the stream
        // - this can mean error on the remote end

        if (readn == 0)
        {
            return traits::eof();
        }

        size_t totalbytes = readn + remained_;
        sbuftype::setg(inbuf_, inbuf_,
            inbuf_ + totalbytes / sizeof(char_type));

        remained_ = totalbytes % sizeof(char_type);
        if (remained_ != 0)
        {
            remainedchar_ = inbuf_[totalbytes / sizeof(char_type)];
        }

        return this->sgetc();
    }

private:

    // not for use
    socket_stream_buffer(const socket_stream_buffer &);
    void operator=(const socket_stream_buffer &);

    socket_wrapper & rsocket_;
    bool ownsocket_;
    char_type * inbuf_;
    char_type * outbuf_;
    std::streamsize bufsize_;
    size_t remained_;
    char_type remainedchar_;
    bool ownbuffers_;
};


// this class is an ultimate stream associated with a socket
template <class socket_wrapper,
          class charT, class traits = std::char_traits<charT> >
class socket_generic_stream :
    private socket_stream_buffer<socket_wrapper, charT, traits>,
    public std::basic_iostream<charT, traits>
{
public:

    // this constructor takes 'ownership' of the socket wrapper if btakeowner == true,
    // so that the socket will be closed in the destructor of the
    // tcp_stream_buffer object
    explicit socket_generic_stream(socket_wrapper & sock, bool takeowner = false)
        : socket_stream_buffer<socket_wrapper, charT, traits>(sock, takeowner),
          std::basic_iostream<charT, traits>(this)
    {
    }

private:
    // not for use
    socket_generic_stream(const socket_generic_stream &);
    void operator=(const socket_generic_stream &);
};

namespace sockets_details
{

// helper type for hiding conflicting names
    template <class isolated>
    struct base_class_isolator
    {
        isolated isedmember_;
    };

} // namespace sockets_details

// specialized for use as a TCP client
template <class charT, class traits = std::char_traits<charT> >
class tcp_generic_client_stream :
    private sockets_details::base_class_isolator<tcp_socket_wrapper>,
    public socket_generic_stream<tcp_socket_wrapper, charT, traits>
{
public:

    tcp_generic_client_stream(const std::string & address, int port)
        : socket_generic_stream<tcp_socket_wrapper, charT, traits>(isedmember_, false)
    {
        isedmember_.connect(address, port);
    }

private:
    // not for use
    tcp_generic_client_stream(const tcp_generic_client_stream &);
    void operator=(const tcp_generic_client_stream &);
};

#ifndef WIN32

// specialized for use as a Unix socket client
template <class charT, class traits = std::char_traits<charT> >
class unix_generic_client_stream :
    private sockets_details::base_class_isolator<unix_socket_wrapper>,
    public socket_generic_stream<unix_socket_wrapper, charT, traits>
{
public:

    unix_generic_client_stream(const std::string & path)
        : socket_generic_stream<unix_socket_wrapper, charT, traits>(isedmember_, false)
    {
        isedmember_.connect(path);
    }

private:
    // not for use
    unix_generic_client_stream(const unix_generic_client_stream &);
    void operator=(const unix_generic_client_stream &);
};

#endif // WIN32

// helper declarations for narrow and wide streams
typedef socket_generic_stream<tcp_socket_wrapper, char> tcp_stream;
typedef socket_generic_stream<tcp_socket_wrapper, wchar_t> tcp_wstream;
typedef tcp_generic_client_stream<char> tcp_client_stream;
typedef tcp_generic_client_stream<wchar_t> tcp_client_wstream;

#ifndef WIN32
typedef socket_generic_stream<unix_socket_wrapper, char> unix_stream;
typedef socket_generic_stream<unix_socket_wrapper, wchar_t> unix_wstream;
typedef unix_generic_client_stream<char> unix_client_stream;
typedef unix_generic_client_stream<wchar_t> unix_client_wstream;
#endif // WIN32

#endif
