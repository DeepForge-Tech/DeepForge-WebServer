//
// This example serves static dist and has two dynamic actions to support AJAX calls.
//

#include "http_server.h"

#include <iostream>

int current_value = 0;

// The up() and down() functions generate just the changing portion of the GUI.
// In real application consider making them thread-safe.

void up(std::ostream & out,
    const std::string & /* path */, const std::string & /* params */)
{
    out << ++current_value;
}

void down(std::ostream & out,
    const std::string & /* path */, const std::string & /* params */)
{
    out << --current_value;
}

int main()
{
    http::register_text_get_action("up", up);
    http::register_text_get_action("down", down);

    http::server_start(12345, "dist", std::cerr);
}
