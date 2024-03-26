#include <http_server.h>

#include <iostream>
#include <vector>

void greet(std::ostream & out,
    const std::string & /* path */, const std::string & /* params */,
    std::istream & in, std::size_t content_length, const std::string & /* content_type */)
{
    // This function is responsible for reading content_length bytes from the in stream.

    std::vector<char> data(content_length);
    in.read(&data[0], content_length);

    http::params_map_type params_map = http::decode_params(data, true);
    
    std::string name = params_map["name"];

    out << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head>\n"
        << "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
        << "  <title>5 Forms</title>\n"
        << "  <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
        << "</head>\n"
        << "<body>\n"
        << "<h1>Example 5 - Forms</h1>\n"
        << "<p>Hello " << http::html_encode(name) << "!</p>\n"
        << "<p>Go back to <a href=\"/\">main page</a>.</p>\n"
        << "</body>\n"
        << "</html>\n";
}

int main()
{
    http::register_html_post_action("greet", greet);

    http::server_start(8000, "dist", std::cerr);
}
