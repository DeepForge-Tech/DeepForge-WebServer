#include "http_server.h"

#include <iostream>

void my_action(std::ostream & out,
    const std::string & /* path */, const std::string & /* params */)
{
    // This function generates the whole HTML content for the /my_action link.
    // The content of this page is dynamic.

    // As you see, typing larger fragments of HTML in C++
    // is cumbersome and not very practical.
    // Still, assembling HTML blocks from external sources (dist, DB, etc.)
    // and processing them with a template parsing engine can be very effective.

    // With small pieces of dynamic content see the AJAX example for a better alternative.

    int random_number = std::rand();

    out << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head>\n"
        << "  <title>2 Dynamic</title>\n"
        << "  <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
        << "</head>\n"
        << "<body>\n"
        << "<h1>Example 2 - Dynamic Content</h1>\n"
        << "<p>This is a random number: " << random_number << "</p>\n"
        << "<p>Refresh at will or go back to <a href=\"/\">main page</a>.</p>\n"
        << "</body>\n"
        << "</html>\n";
}

int main()
{
    // "/my_action" is the URL for dynamic content.
    // my_action is the function (above) that generates the dynamic content.
    // The browser normally asks for the content using GET method.

    http::register_html_get_action("my_action", my_action);

    http::server_start(8000, "dist", std::cerr);
}
