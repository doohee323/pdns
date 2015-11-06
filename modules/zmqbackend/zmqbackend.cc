#include <string>
#include <map>
#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <queue>
#include <list>

#include "pdns/namespaces.hh"

#include <pdns/dns.hh>
#include <pdns/dnsbackend.hh>
#include <pdns/dnspacket.hh>
#include <pdns/ueberbackend.hh>
#include <pdns/logger.hh>
#include <pdns/arguments.hh>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <boost/lexical_cast.hpp>
#include "zmqbackend.hh"

static const char *kBackendId = "[ZMQBackend]";
zmq::context_t *ZMQBackend::zmq_context = NULL;

//
// constructor:
//	check arguments
//	call connect
//
// throws:
//	ArgException: on bad arg
//

ZMQBackend::ZMQBackend(const string &sfx)
{
	try
	{
		suffix = sfx;
		setArgPrefix("zmq" + suffix);

		version = getArgAsNum("version");

		if (version < 1 || version > 2)
		{
			throw ArgException("zmq" + suffix + "version can not be < 1 or > 2");
		}

		zmq_url	    = getArg("url");

		if (zmq_url == "")
		{
			throw ArgException("zmq" + suffix + "url cannot be empty");
		}

		//
		// convert from ms to ns for poll call
		//
		zmq_timeout = getArgAsNum("timeout") * 1000;

		if (zmq_timeout <= 0)
		{
			throw ArgException("zmq" + suffix + "timeout can not be 0 or negative");
		}

		do_query_logging = ::arg().mustDo("query-logging");

		do_wildcards = false;

		if (getArgAsNum("do-wildcards") == 1)
		{
			do_wildcards = true;
		}

		zmq_socket = NULL;

		if (NULL == zmq_context)
		{
			zmq_context = new zmq::context_t(1);
		}
	}
	catch (const ArgException &e)
	{
		throw;
	}
}

ZMQBackend::~ZMQBackend()
{
	L << Logger::Info << kBackendId << " calling destructor" << endl;

	if (zmq_socket)
	{
		zmq_socket->close();
		delete zmq_socket;
		zmq_socket = NULL;
	}
}

//
// send: sends the given line to the backend as a query to be answered
//
// throws: 
//	ZException on any error
//
void ZMQBackend::send(const string &line)
{

}

//
// receive: receives the response message from the socket and puts
//	it in the given string passed in as the function arg.
//	this is a simple wrapper around the receive with poll logic.
//
// NOTE: the response is expected to be multiple lines combined, to
// 	be split on "\n" by the caller.
//
// side-effects: none
//
// throws:
//	ZException: on any error, including attempt to receive() on a connection
//		that has not had a request sent(), which is a zmq exception
//
void ZMQBackend::receive(string &line)
{
}

//
// lookup: the primary entry point for making queries.  These are almost always
//	SOA or ANY queries
//
// throws:
//	nothing
//
// side-effects:
//	destroys zmq_socket on any error so next call to lookup() will re-create it
//	any calls to get() will return false if that has happened,
//	and if the call to lookup() succeeded, get() will access the results in $lineQueue
//
void ZMQBackend::lookup( const QType& qtype, const DNSName& qname, DNSPacket* dnspkt, int zoneid )
{
}

bool ZMQBackend::getSOA(const DNSName& name, SOAData& soadata, DNSPacket*)
{
	return true;
}

//
// get is expected to be called after a list or lookup call
// expect this to be called repeatedly until it returns false
// each time it is xpected to fill out the DNSResourceRecord
// content.  It should return false when it has no more responses
// to return (e.g. it has seen "END")
//
// throws:
//	nothing
//
bool ZMQBackend::get(DNSResourceRecord &rr)
{
	return false;
}
	
//
// list and lookup are the two routines that send requests to the socket
// and save metadata in the d_qname and d_qtype private parameters so when
// the caller of list or lookup call get() to retrieve the answer, we know
// how to process the line coming back from the socket
//
// throws:
// 	nada
//
bool ZMQBackend::list( const DNSName& target, int zoneid, bool include_disabled )
{
	return true;
}

//
// static factory method
//
DNSBackend *ZMQBackend::maker()
{
	try
	{
		return new ZMQBackend();
	}
	catch (...)
	{
		L << Logger::Error << kBackendId << " Unable to instantiate a zmqbackend!" << endl;
		return NULL;
	}
}

//
// Magic class that is activated when the dynamic library is loaded
//

class ZMQFactory : public BackendFactory
{
	public:
		ZMQFactory() : BackendFactory("zmq") {}

		void declareArguments(const string &suffix = "")
		{
			declare(suffix, "url", "zmq URL for backend to pipe questions to", "");
			declare(suffix, "timeout", "Number of milliseconds to wait for an answer", "1000");
			declare(suffix, "version", "Version for backend protocol", "1");
			declare(suffix, "do-wildcards", "set to 0 to disable wildcard querying, 1 to enable", "0");
		}

		DNSBackend *make(const string &suffix = "")
		{
			return new ZMQBackend(suffix);
		}
};

class ZMQLoader
{
	public:
		ZMQLoader()
		{
			BackendMakers().report(new ZMQFactory);

			//L << Logger::Notice << kBackendId << " This is the zmqbackend version "VERSION" ("__DATE__", "__TIME__") reporting"; << endl;
		}
};

static ZMQLoader zmqbackend;

