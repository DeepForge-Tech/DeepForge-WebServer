
#include <http_server.h>
#include <sockets.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>

#include <mutex>
#include <thread>

using namespace http;

namespace // unnamed
{

std::ostream * logger;
unsigned int log_mask;

int listening_port;
std::string base_dir;

connection_callback_type connection_callback;

// stores action functions and their known mime_types
// (or "" if registered as generic action)
std::unordered_map<std::string, std::pair<get_action_type, std::string> > get_actions;
std::unordered_map<std::string, std::pair<post_action_type, std::string> > post_actions;

std::mutex mtx;

int hex_digit_to_int(char c)
{
    int t = (int)c;
    int result;
    
    if ((t >= (int)'0') && (t <= (int)'9'))
    {
        result = t - (int)'0';
    }
    else if ((t >= (int)'a') && (t <= (int)'f'))
    {
        result = t - (int)'a' + 10;
    }
    else if ((t >= (int)'A') && (t <= (int)'F'))
    {
        result = t - (int)'A' + 10;
    }
    else
    {
        // arbitrary failover value
        result = 0;
    }
    
    return result;
}

std::string file_mime_type(const std::string & file_name)
{
    std::size_t pos = file_name.find('.');
    std::string ext = file_name.substr(pos + 1);
    if (ext == "html")
    {
        return "text/html";
    }
    else if (ext == "css")
    {
        return "text/css";
    }
    else if (ext == "js")
    {
        return "application/javascript";
    }
    else if (ext == "png")
    {
        return "image/png";
    }
    else if (ext == "jpg")
    {
        return "image/jpg";
    }
    else
    {
        return "text/plain";
    }
}

params_map_type do_decode_params(const char * begin, const char * end, bool decode)
{
    enum decoder_state { key, value };
    decoder_state state = key;
    
    std::string k;
    std::string v;
    
    params_map_type result;
    
    for (const char * it = begin; it != end; ++it)
    {
        char c = *it;
        
        switch (state)
        {
        case key:
            if (c == '=')
            {
                state = value;
            }
            else
            {
                k.append(1, c);
            }

            break;

        case value:
            if (c == '&')
            {
                if (decode)
                {
                    result[url_decode(k)] = url_decode(v);
                }
                else
                {
                    result[k] = v;
                }

                k.clear();
                v.clear();

                state = key;
            }
            else
            {
                v.append(1, c);
            }

            break;
        }
    }

    if (state == value)
    {
        if (decode)
        {
            result[url_decode(k)] = url_decode(v);
        }
        else
        {
            result[k] = v;
        }
    }
    
    return result;
}

void get_file(std::ostream & out, const std::string & file_name)
{
    if ((logger != NULL) && ((log_mask & log_static_requests) != 0))
    {
        std::lock_guard<std::mutex> lck(mtx);
        
        *logger << "GET file " << file_name << '\n';
    }

    std::ifstream file(base_dir + file_name, std::ifstream::binary);

    if (file)
    {
        std::filebuf * pbuf = file.rdbuf();
        
        std::size_t size = pbuf->pubseekoff(0, file.end, file.in);
        pbuf->pubseekpos(0, file.in);
        
        std::vector<char> buffer(size);
        
        pbuf->sgetn(&buffer[0], size);
        
        file.close();
        
        out << header(file_mime_type(file_name), size, true);
        out.write(&buffer[0], size);

        out << "\r\n";

        out.flush();

        if ((logger != NULL) && ((log_mask & log_static_responses) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);
            
            *logger << "file " << file_name << " size " << size << " bytes was sent\n";
        }
    }
    else
    {
        if ((logger != NULL) && ((log_mask & log_static_requests) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);
            
            *logger << "file not found: " << file_name << '\n';
        }
        
        out << "HTTP/1.1 404 Not Found\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: 0\r\n\r\n";

        out.flush();
    }
}

void get_action(std::ostream & out,
    const get_action_type & action, const std::string & mime_type,
    const std::string & path, const std::string & params)
{
    try
    {
        if ((logger != NULL) && ((log_mask & log_dynamic_requests) != 0))
        {
            *logger << "GET action " << path << '\n';
        }

        if (mime_type.empty() == false)
        {
            // collect content to buffer
            // and automatically generate appropriate HTTP header,
            // depending on the registered mime_type and size of collected content

            std::ostringstream str_buf;
                    
            action(str_buf, path, params);
                    
            out << header(mime_type, str_buf.str().size(), false);
            out << str_buf.str();

            if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
            {
                *logger << "GET action " << path
                    << " of type " << mime_type
                    << " returned " << str_buf.str().size() << " bytes\n";
            }
            
            out.flush();
        }
        else
        {
            // allow the user to generate both the header and content
                    
            action(out, path, params);
                    
            if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
            {
                *logger << "generic GET action " << path << " executed\n";
            }
        }
    }
    catch (const std::exception & e)
    {
        if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);
                    
            *logger << "error in GET action " << path << ": " << e.what() << '\n';
        }
    }
    catch (...)
    {
        if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);
                    
            *logger << "unknown error in GET action " << path << '\n';
        }
    }
}

void get(std::ostream & out, const std::string & what)
{
    std::string path;
    std::string params;
    std::size_t pos = what.find('?');
    if (pos != std::string::npos)
    {
        path = what.substr(0, pos);
        params = what.substr(pos + 1);
    }
    else
    {
        path = what;
    }

    if (path == "/")
    {
        get_file(out, "/index.html");
    }
    else
    {
        get_action_type action;
        std::string mime_type;
        
        bool found = false;

        {
            std::lock_guard<std::mutex> lck(mtx);

            auto it = get_actions.find(path);
            if (it != get_actions.end())
            {
                action = it->second.first;
                mime_type = it->second.second;

                found = true;
            }
        }

        if (found)
        {
            get_action(out, action, mime_type, path, params);
        }
        else
        {
            get_file(out, path);
        }
    }
}

void post(std::ostream & out, const std::string & what,
    std::istream & in, std::size_t content_length, const std::string & content_type)
{
    std::string path;
    std::string params;
    std::size_t pos = what.find('?');
    if (pos != std::string::npos)
    {
        path = what.substr(0, pos);
        params = what.substr(pos + 1);
    }
    else
    {
        path = what;
    }

    post_action_type action;
    std::string mime_type;
    
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lck(mtx);
        
        auto it = post_actions.find(path);
        if (it != post_actions.end())
        {
            action = it->second.first;
            mime_type = it->second.second;
            
            found = true;
        }
    }
    
    if (found)
    {
        try
        {
            if ((logger != NULL) && ((log_mask & log_dynamic_requests) != 0))
            {
                *logger << "POST action " << path << '\n';
            }
            
            if (mime_type.empty() == false)
            {
                // collect content to buffer
                // and automatically generate appropriate HTTP header,
                // depending on the registered mime_type and size of collected content
                
                std::ostringstream str_buf;
                
                action(str_buf, path, params, in, content_length, content_type);
                
                out << header(mime_type, str_buf.str().size(), false);
                out << str_buf.str();

                out.flush();
                
                if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
                {
                    *logger << "POST action " << path
                        << " of type " << mime_type
                        << " returned " << str_buf.str().size() << " bytes\n";
                }
            }
            else
            {
                // allow the user to generate both the header and content
                
                action(out, path, params, in, content_length, content_type);
                
                if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
                {
                    *logger << "generic POST action " << path << " executed\n";
                }
            }
        }
        catch (const std::exception & e)
        {
            if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
            {
                std::lock_guard<std::mutex> lck(mtx);
                
                *logger << "error in POST action " << path << ": " << e.what() << '\n';
            }
        }
        catch (...)
        {
            if ((logger != NULL) && ((log_mask & log_dynamic_responses) != 0))
            {
                std::lock_guard<std::mutex> lck(mtx);
                
                *logger << "unknown error in POST action " << path << '\n';
            }
        }
    }
}

void connection_thread(std::shared_ptr<tcp_socket_wrapper> sock)
{
    if ((logger != NULL) && ((log_mask & log_connections) != 0))
    {
        std::lock_guard<std::mutex> lck(mtx);
        
        *logger << "accepted new connection\n";
    }
    
    try
    {
        tcp_stream stream(*sock);
        
        if (connection_callback != nullptr)
        {
            try
            {
                connection_callback(stream, just_connected);
            }
            catch (...)
            {
                // ignore
            }
        }
        
        std::string line;
        bool get_command = false;
        bool post_command = false;
        std::string resource;
        std::size_t content_length;
        std::string content_type;

        while (std::getline(stream, line))
        {
            if (line[line.size() - 1] == '\r')
            {
                line = line.substr(0, line.size() - 1);
            }

            if (line == "")
            {
                if (get_command)
                {
                    get(stream, resource);
                }
                else if (post_command)
                {
                    post(stream, resource, stream, content_length, content_type);
                }

                get_command = false;
                post_command = false;
            }
            else
            {
                if ((get_command == false) && (post_command == false))
                {
                    if (line.substr(0, 3) == "GET")
                    {
                        get_command = true;

                        std::size_t pos = line.find(' ', 4);
                        if (pos != std::string::npos)
                        {
                            resource = line.substr(4, pos - 4);
                        }
                    }
                    else if (line.substr(0, 4) == "POST")
                    {
                        post_command = true;

                        std::size_t pos = line.find(' ', 5);
                        if (pos != std::string::npos)
                        {
                            resource = line.substr(5, pos - 5);
                        }
                    }
                }
                else if (line.substr(0, 15) == "Content-Length:")
                {
                    unsigned long t;
                    (void)std::sscanf(line.c_str() + 15, "%lu", &t);
                    content_length = (std::size_t)t;
                }
                else if (line.substr(0, 13) == "Content-Type:")
                {
                    content_type = line.substr(14);
                }
            }
        }

        if (connection_callback != nullptr)
        {
            try
            {
                connection_callback(stream, to_be_closed);
            }
            catch (...)
            {
                // ignore
            }
        }

        if ((logger != NULL) && ((log_mask & log_connections) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);

            *logger << "finished with this connection\n";
        }
    }
    catch (const std::exception & e)
    {
        if ((logger != NULL) && ((log_mask & log_connections) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);

            *logger << "error in connection thread: " << e.what() << '\n';
        }
    }
}

} // unnamed namespace

void http::server_start(int port_number, const char * base_directory)
{
#ifdef _WIN32
    WORD versionRequested = MAKEWORD(2, 0);
    WSADATA wsadata;
    (void)WSAStartup(versionRequested, &wsadata);
#endif

    listening_port = port_number;
    base_dir = base_directory;
    
    try
    {
        tcp_socket_wrapper sockserver;

        sockserver.listen(port_number);

        if ((logger != NULL) && ((log_mask & log_connections) != 0))
        {
            *logger << "HTTP server is listening on port " << port_number << '\n';
        }

        while (true)
        {
            std::shared_ptr<tcp_socket_wrapper> sock(
                new tcp_socket_wrapper(sockserver.accept()));

            std::thread th(connection_thread, sock);
            th.detach();
        }
    }
    catch (const std::exception & e)
    {
        if ((logger != NULL) && ((log_mask & log_connections) != 0))
        {
            std::lock_guard<std::mutex> lck(mtx);

            *logger << "HTTP server error: " << e.what() << '\n';
        }
    }
}

void http::server_start(int port_number, const char * base_directory,
    std::ostream & error_log, unsigned int log_events_mask)
{
    logger = &error_log;
    log_mask = log_events_mask;
    
    server_start(port_number, base_directory);
}

void http::register_connection_callback(connection_callback_type callback)
{
    std::lock_guard<std::mutex> lck(mtx);
    
    connection_callback = callback;
}

void http::register_generic_get_action(const char * name, get_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    get_actions[std::string("/") + name] = std::make_pair(f, std::string(""));
}

void http::register_html_get_action(const char * name, get_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    get_actions[std::string("/") + name] = std::make_pair(f, std::string("text/html"));
}

void http::register_text_get_action(const char * name, get_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    get_actions[std::string("/") + name] = std::make_pair(f, std::string("text/plain"));
}

void http::register_generic_post_action(const char * name, post_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    post_actions[std::string("/") + name] = std::make_pair(f, std::string(""));
}

void http::register_html_post_action(const char * name, post_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    post_actions[std::string("/") + name] = std::make_pair(f, std::string("text/html"));
}

void http::register_text_post_action(const char * name, post_action_type f)
{
    std::lock_guard<std::mutex> lck(mtx);

    post_actions[std::string("/") + name] = std::make_pair(f, std::string("text/plain"));
}

std::string http::html_encode(const std::string & s)
{
    std::string result;

    for (char c : s)
    {
        if (c == '<')
        {
            result.append("&lt;");
        }
        else if (c == '>')
        {
            result.append("&gt;");
        }
        else if (c == '&')
        {
            result.append("&amp;");
        }
        else
        {
            result.append(1, c);
        }
    }

    return result;
}

std::string http::url_encode(const std::string & s)
{
    std::string result;
    
    for (char c : s)
    {
        if ((std::isalnum(c) != 0) ||
            (c == '-') || (c == '_') || (c == '.') || (c == '~'))
        {
            result.append(1, c);
        }
        else if (c == ' ')
        {
            result.append(1, '+');
        }
        else
        {
            char buf[4];
            
            std::sprintf(buf, "%%%x", (int)c);
            
            result.append(buf);
        }
    }
    
    return result;
}

std::string http::url_decode(const std::string & s)
{
    enum decoder_state { regular, percent_1, percent_2 };
    decoder_state state = regular;
    int hex_1, hex_2;
    
    std::string result;
    
    for (char c : s)
    {
        switch (state)
        {
        case regular:
            if (c == '%')
            {
                state = percent_1;
            }
            else if (c == '+')
            {
                result.append(1, ' ');
            }
            else
            {
                result.append(1, c);
            }
            
            break;
            
        case percent_1:
            hex_1 = hex_digit_to_int(c);
            state = percent_2;
            
            break;
            
        case percent_2:
            hex_2 = hex_digit_to_int(c);
            
            result.append(1, (char)(16 * hex_1 + hex_2)); 
            
            state = regular;
            
            break;
        }
    }
    
    return result;
}

params_map_type http::decode_params(const std::string & params, bool decode)
{
    const char * begin = params.data();
    const char * end = begin + params.size();

    return do_decode_params(begin, end, decode);
}

params_map_type http::decode_params(const std::vector<char> & params, bool decode)
{
    const char * begin = &params[0];
    const char * end = begin + params.size();

    return do_decode_params(begin, end, decode);
}

std::string http::header(const std::string & mime_type,
    std::size_t content_length, bool cache)
{
    std::string res = std::string("HTTP/1.1 200 OK\r\n")
        + "Content-Type: " + mime_type + "\r\n"
        + (content_length != 0 ?
            "Content-Length: " + std::to_string(content_length) + "\r\n" :
            "")
        + (cache ?
            "Cache-Control: public, max-age=31536000\r\n" :
            "Cache-Control: no-cache, no-store, must-revalidate\r\n")
        + "\r\n";

    return res;
}
