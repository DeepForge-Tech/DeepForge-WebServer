//
// This example serves updates via server-side events.
//

#include "http_server.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

int current_value = 0;
std::ostream * updates = NULL;
std::mutex mtx;

void get_updates(std::ostream & out,
    const std::string & /* path */, const std::string & /* params */)
{
    std::lock_guard<std::mutex> lck(mtx);

    updates = &out; // just remember where to push updates
}

void connection_callback(std::ostream &, http::connection_event event)
{
    if (event == http::to_be_closed)
    {
        std::lock_guard<std::mutex> lck(mtx);
        
        updates = NULL; // the stream is closing, prevent further pushes
    }
}

void activity()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        {
            std::lock_guard<std::mutex> lck(mtx);

            if (updates != NULL)
            {
                // somebody is listening, push the update

                *updates << http::header("text/event-stream")
                    << "data: " << ++current_value << "\r\n\r\n"
                    << std::flush;
            }
        }
    }
}

int main()
{
    http::register_generic_get_action("get_updates", get_updates);
    http::register_connection_callback(connection_callback);

    std::thread th(activity);

    http::server_start(12345, "dist", std::cerr);
}
