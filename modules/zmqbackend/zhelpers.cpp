#include "zhelpers.h"

using namespace zmq;

//  Receive 0MQ string from socket and convert into string
std::string zmq::s_recv (zmq::socket_t & socket) {

    zmq::message_t message;
    socket.recv(&message);

    return std::string(static_cast<char*>(message.data()), message.size());
}

//  Convert string to 0MQ string and send to socket
bool zmq::s_send (zmq::socket_t & socket, const std::string & string, const int flags) {

    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());

    bool rc = socket.send(message, flags);
    return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
bool zmq::s_sendmore (zmq::socket_t & socket, const std::string & string) {

    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());

    bool rc = socket.send(message, ZMQ_SNDMORE);
    return (rc);
}

//  Receives all message parts from socket, prints neatly
//
void zmq::s_dump (zmq::socket_t & socket)
{
    std::cout << "----------------------------------------" << std::endl;

    while (1) {
	//  Process all parts of the message

	zmq::message_t message;
	socket.recv(&message);

	//  Dump the message as text or binary
	std::string data(static_cast<char*>(message.data()));
	int size = message.size();

	bool is_text = true;

	int char_nbr;
	unsigned char byte;
	for (char_nbr = 0; char_nbr < size; char_nbr++) {
	    byte = data [char_nbr];
	    if (byte < 32 || byte > 127)
	      is_text = false;
	}

	std::cout << std::setfill('0') << std::setw(3) << "[" << size << "]";

	for (char_nbr = 0; char_nbr < size; char_nbr++) {
	    if (is_text) {
		std::cout << (char)data [char_nbr];
	    } else {
		std::cout << std::setfill('0') << std::setw(2)
		   << std::hex << (unsigned char) data [char_nbr];
	    }
	}
	std::cout << std::endl;

	int64_t more;	   //  Multipart detection
	size_t more_size = sizeof (more);
	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);

	if (!more)
	    break;      //  Last message part
    }
}

//  Set simple random printable identity on socket
//
std::string
zmq::s_set_id (zmq::socket_t & socket)
{
    std::stringstream ss;
    ss << std::hex << std::uppercase
	  << std::setw(4) << std::setfill('0') << within (0x10000) << "-"
	  << std::setw(4) << std::setfill('0') << within (0x10000);
    socket.setsockopt(ZMQ_IDENTITY, ss.str().c_str(), ss.str().length());
    return ss.str();
}

//  Report 0MQ version number
//
void zmq::s_version (void)
{
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    std::cout << "Current 0MQ version is " << major << "." << minor << "." << patch << std::endl;
}

void zmq::s_version_assert (int want_major, int want_minor)
{
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    if (major < want_major
    || (major == want_major && minor < want_minor)) {
	std::cout << "Current 0MQ version is " << major << "." << minor << std::endl;
	std::cout << "Application needs at least " << want_major << "." << want_minor
	      << " - cannot continue" << std::endl;
	exit (EXIT_FAILURE);
    }
}

//  Return current system clock as milliseconds
int64_t zmq::s_clock (void)
{
#if (defined (__WINDOWS__))
    SYSTEMTIME st;
    GetSystemTime (&st);
    return (int64_t) st.wSecond * 1000 + st.wMilliseconds;
#else
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

//  Sleep for a number of milliseconds
void zmq::s_sleep (int msecs)
{
#if (defined (__WINDOWS__))
    Sleep (msecs);
#else
    struct timespec t;
    t.tv_sec = msecs / 1000;
    t.tv_nsec = (msecs % 1000) * 1000000;
    nanosleep (&t, NULL);
#endif
}
