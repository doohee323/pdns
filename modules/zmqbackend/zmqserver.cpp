//
// Lazy Pirate server
// Binds REQ socket to tcp://*:5555
// Like hwserver except:
// - echoes request as-is
// - randomly runs slowly, or exits to simulate a crash.
//
#include "zhelpers.hpp"

int main ()
{
    srandom ((unsigned) time (NULL));

    zmq::context_t context(1);
    zmq::socket_t server(context, ZMQ_REP);
    server.bind("tcp://*:9999");

    while (1) {
        std::string request = s_recv (server);
        std::cout << "request: (" << request << ")" << std::endl;
        if(request == "HELO\t2") {
        	s_send (server, "OK");
            sleep (2);
        } else {
            s_send (server, "RESULT");
        }
    }
    return 0;
}
