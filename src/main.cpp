//
// This example serves static dist and has one dynamic action.
//

#include <http_server.h>

#include <iostream>
using namespace std;
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
    // "/my_action" is the URL for dynamic content.
    // my_action is the function (above) that generates the dynamic content.
    // The browser normally asks for the content using GET method.

    http::register_html_get_action("my_action", my_action);
    http::register_html_post_action("my_action", greet);

    http::server_start(8000, "dist", std::cerr);
}
