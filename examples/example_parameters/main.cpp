#include "http_server.h"

#include <iostream>
#include <string>

void calculate(std::ostream & out,
    const std::string & /* path */, const std::string & params)
{
    // Note: this code assumes that the URL has the expected parameters.
    // Consider validating the input in real application.

    http::params_map_type params_map = http::decode_params(params, false);
    
    int x = std::stoi(params_map["x"]);
    int y = std::stoi(params_map["y"]);

    int new_random_x = std::rand() % 100;
    int new_random_y = std::rand() % 100;

    // Note: when generating more complex links, think about http::url_encode().

    std::string next_url = "/calculate?x=" + std::to_string(new_random_x) +
        "&y=" + std::to_string(new_random_y);

    out << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head>\n"
        << "  <title>3 Parameters</title>\n"
        << "  <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
        << "</head>\n"
        << "<body>\n"
        << "<h1>Example 3 - Dynamic Content With Parameters</h1>\n"
        << "<p>x = " << x << "</p>\n"
        << "<p>y = " << y << "</p>\n"
        << "<p>x + y = " << (x + y) << "</p>\n"
        << "<p>Modify the URL by hand or try this lucky link: <a href=\""
            << next_url << "\">" << next_url << "</a>.</p>\n"
        << "</body>\n"
        << "</html>\n";
}

int main()
{
    http::register_html_get_action("calculate", calculate);

    http::server_start(8000, "dist", std::cerr);
}
