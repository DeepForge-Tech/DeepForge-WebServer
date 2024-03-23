//
// This is the basic example serving static dist.
// Point your browser to http://localhost:12345
//

#include "http_server.h"

int main()
{
    // 12345 is the port number,
    // ./dist is the directory with static dist (index.html, style.css)

    // Note: server_start does not return as long as it functions properly.

    http::server_start(12345, "dist");

    // this will print log messages to selected output stream:
    //http::server_start(12345, "dist", std::cerr);
}
