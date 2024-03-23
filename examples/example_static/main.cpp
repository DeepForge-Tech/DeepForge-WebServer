#include "http_server.h"

int main()
{
    // 8000 is the port number,

    // Note: server_start does not return as long as it functions properly.

    http::server_start(8000, "dist");

    // this will print log messages to selected output stream:
    //http::server_start(12345, "dist", std::cerr);
}
