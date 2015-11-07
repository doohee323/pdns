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
	//
	// send to zmq socket
	// blocking send
	// if the socket is not in the right mode to send, a zmq::error_t (std::exception)
	// 	will be thrown and we should attempt to reconnect and re-send
	//
	if (!zmq_socket)
	{
		throw ZException("can't call send on a null zmq_socket");
	}

	try
	{
		//
		// returns false on EAGAIN from socket, which shouldn't ever occur
		// since we're not using nonblocking sockets...
		//
		if (!s_send(*zmq_socket, line))
		{
			throw ZException("send returned false and we're not using nonblocking sockets");
		}
	}
	catch (const std::exception &e)
	{
		throw ZException(e.what());
	}
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
	try
	{
		//
		// receive line from zmq socket
		//
		zmq::pollitem_t items[] =
		{
			{ zmq_socket, 0, ZMQ_POLLIN, 0 }
		};
		zmq::poll(&items[0], 1, (long)zmq_timeout);

		if (items[0].revents & ZMQ_POLLIN)
		{
			line = s_recv(*zmq_socket);
			return;
		}
	}
	catch (const std::exception &e)
	{
		L << Logger::Error << kBackendId << " caught exception in receive: " << e.what() << endl;
		throw ZException(e.what());
	}

	//
	// got here because no response within timeout seconds, throw
	// exception on assumption that the backend is foobar or the
	// socket is blown, so we'll restart a new backend and thus a new
	// connection.
	//
	// it is wise to set timeout high enough that you don't tear down
	// and set up new connections continuously, but not so high that
	// you give poor performance.
	//
	throw ZException("socket timeout");
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
void ZMQBackend::lookup( const QType& qtype, const DNSName& qname, DNSPacket* pkt_p, int zoneId )
{
	//
	// reset all state variables to a clean state and disconnect
	// any previously connected socket and delete any prior zmq context
	//
	while (!lineQueue.empty())
	{
		lineQueue.pop();
	}
	
	//
	// there's no way to turn off wildcard support in pdns itself, so we have to intercept it here
	//
	if (!do_wildcards && qname.toString().compare("*"))
	{
		return;
	}

	//
	// if ! connected, connect
	//

	if (NULL == zmq_socket)
	{
		try
		{
			//
			// this end of the zmq socket is a ZMQ_REQ socket, the other should be
			// a ROUTER socket or a REP socket.
			//
			zmq_socket  = new zmq::socket_t(*zmq_context, ZMQ_REQ);
	
			uint64_t hwm = 1;
			int linger_time = 1;
	
			zmq_socket->setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
			zmq_socket->setsockopt(ZMQ_LINGER, &linger_time, sizeof(linger_time));
	
			zmq_socket->connect(zmq_url.c_str());
	
			//
			// leaving zmq-version in to support versioning of backends
			//
			send("HELO\t" + getArg("version"));
			string banner = "";
			receive(banner);
	
			if (banner != "OK")
			{
				throw ZException("version disagreement with backend, it does not like version " + getArg("version"));
			}
	
			L << Logger::Info << "Backend " << suffix << " launched with banner: " << banner << endl;
		}
		catch (const ArgException &A)
		{
			L << Logger::Error << kBackendId << " Fatal argument error: " << A.reason << endl;
			if (zmq_socket)
			{
				zmq_socket->close();
				delete zmq_socket;
				zmq_socket = NULL;
			}
			return;
		}
		catch (const std::exception &e)
		{
			L << Logger::Error << kBackendId << " Fatal argument error: " << e.what() << endl;
			if (zmq_socket)
			{
				zmq_socket->close();
				delete zmq_socket;
				zmq_socket = NULL;
			}
			return;
		}
		catch (ZException &e)
		{
			L << Logger::Error << kBackendId << " Fatal argument error: " << e.what() << endl;
			if (zmq_socket)
			{
				zmq_socket->close();
				delete zmq_socket;
				zmq_socket = NULL;
			}
			return;
		}
	}

	//
	// we are now connected or we have returned and lineQueue is empty
	//

	string localIP  = "0.0.0.0";
	string remoteIP = "0.0.0.0";
	Netmask realRemote("0.0.0.0/0");

	if (pkt_p)
	{
		localIP    = pkt_p->getLocal();
		remoteIP   = pkt_p->getRemote();
		realRemote = pkt_p->getRealRemote();
	}

	//
	// zmq-version: 1
	//
	// qname	   qtype   	id      remote-ip-address	local-ip	realRemoteIP
	// $hostname	   A|MX|ANY|etc	id	remote ip address	local ip	remote ip address
	//
	// zmq-version: 2
	//
	// qname	   qtype   	id      remote-ip-address	local-ip	realRemoteIP		real-request-type
	// $hostname	   A|MX|ANY|etc	id	remote ip address	local ip	remote ip address	ANY, CNAME, AAAA, A, MX, etc
	//

	ostringstream query;

	if (version == 1)
	{
		query << qname << "\t" << qtype.getName() << "\t" << zoneId << "\t" << remoteIP << "\t" << localIP << "\t" << realRemote.toString();
	}
	else if (version == 2)
	{
		query << qname << "\t" << qtype.getName() << "\t" << zoneId << "\t" << remoteIP << "\t" << localIP << "\t" << realRemote.toString()
			<< "\t" << pkt_p->qtype.getName();
	}

	if (do_query_logging)
	{
		L << Logger::Debug << "Query: '" << query.str() << "'" << endl;
	}

	d_qtype = qtype;
	d_qname = qname.toString();

	try
	{
		string response;

		send(query.str());
		receive(response);

		//
		// receive throws an exception on any error or failure, including socket timeout.
		// if we got HERE, it means there was no problem receiving and we should have something
		// in $response.
		// for the case where the client (us) gets in a bad state because the server (backend)
		// was restarted, timing out, throwing an exception, and closing the socket to teh server
		// in the exception handling code is a good way to work around that, because zmq won't
		// get our socket back into the right state again by itself.
		//

		DLOG(L << Logger::Debug << kBackendId << " backend returned response: " << response << endl);
	
		if (response == "")
		{
			L << Logger::Error << kBackendId << " backend returned empty line in query for " << d_qname << endl;
			throw ZException("Format error communicating with backend");
		}

		//
		// populate the lineQueue for get()
		//
		vector<string> lines;
		stringtok(lines, response, "\n");
	
		BOOST_FOREACH(const string& line, lines)
		{
			lineQueue.push(line);
		}
	}
	catch (ZException &e)
	{
		L << Logger::Info << "error sending query to backend: " << e.what() << endl;
		if (zmq_socket)
		{
			zmq_socket->close();
			delete zmq_socket;
			zmq_socket = NULL;
		}
		return;
	}
	catch (const std::exception &e)
	{
		L << Logger::Info << "error sending query to backend: " << e.what() << endl;
		if (zmq_socket)
		{
			zmq_socket->close();
			delete zmq_socket;
			zmq_socket = NULL;
		}
		return;
	}
	catch (...)
	{
		L << Logger::Info << "unknown exception caught for error sending query to backend" << endl;
		if (zmq_socket)
		{
			zmq_socket->close();
			delete zmq_socket;
			zmq_socket = NULL;
		}
		return;
	}
}

bool ZMQBackend::getSOA(const DNSName& name, SOAData& soadata, DNSPacket*)
{
	if (name.empty())
	{
		return false;
	}

	soadata.domain_id = 1;

	soadata.nameserver = DNSName("r1.3crowd.com");
	soadata.hostmaster = DNSName("hostmaster.r1.3crowd.com");
	soadata.serial = 3;

	soadata.refresh = 60;
	soadata.retry = 60;
	soadata.expire = 60;
	soadata.ttl  = 60;
	soadata.default_ttl = 60;

	soadata.db = this;
	soadata.scopeMask = (uint8_t)0;

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
bool ZMQBackend::get(DNSResourceRecord &r)
{
	if (lineQueue.empty())
	{
		return false;
	}

	//
	// iterate over lines until we find a good one or END
	//
	while (!lineQueue.empty())
	{
		string line = lineQueue.front();
		lineQueue.pop();
	
		DLOG(L << Logger::Debug << "ZMQBackend::get got line: " << line << endl);
	
		//
		// The answer format:
		// DATA    qname	   qclass  qtype   ttl     id      content
		//
	
		vector<string>parts;
		stringtok(parts, line, "\t");
	
		if (parts.empty())
		{
			L << Logger::Info << kBackendId << " backend returned empty line in query for " << d_qname << endl;
			continue;
		}
		else if (parts[0] == "END")
		{
			return false;
		}
		else if (parts[0] == "DATA")   // yay
		{
			if (parts.size() < 9)
			{
				L << Logger::Info << kBackendId << " backend returned incomplete or empty line in data section for query for " << d_qname << endl;
				continue;
			}
	
			/*
			 * Fields list:
			 *
	
			0: DATA
			1: scopeMask - used for edns subnet responses
			2: auth
			3: qname
			4:
			5: qtype
			6: ttl
			7: domain_id
	
			If MX|SRV:
	
			8: priority
			9: content
	
			else:
	
			8...: content
		
			*/
	
			//
			// DATA		= parts[0];
			//
			r.scopeMask	= atoi(parts[1].c_str());
			r.auth		= atoi(parts[2].c_str());
			r.qname		= DNSName(parts[3]);
			//
			// IN		= parts[4];
			//
			r.qtype		= parts[5];
			r.ttl		= atoi(parts[6].c_str());
			r.domain_id	= atoi(parts[7].c_str());
		
			if (r.qtype.getCode() == QType::MX || r.qtype.getCode() == QType::SRV)
			{
				if (parts.size() < 10)
				{
					L << Logger::Info << kBackendId << " backend returned incomplete MX/SRV line in data section for query for " << d_qname << endl;
					continue;
				}
	
				// r.priority = atoi(parts[8].c_str());
				r.content = parts[9];
			}
			else
			{
				r.content.clear();
	
				for (unsigned int n = 8; n < parts.size(); ++n)
				{
					if (n != 8)
					{
						r.content.append(1, ' ');
					}
					r.content.append(parts[n]);
				}
			}

			return true;
		}
		else
		{
			L << Logger::Info << "ZMQprocess backend sent incorrect response '" << line << "'";
			continue;
		}
	}
	
	L << Logger::Info << "ran out of lines to process while looking for a good response for query " << d_qname << endl;
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
bool ZMQBackend::list( const DNSName& target, int inZoneId, bool include_disabled )
{
	//
	// The question format:
	// qname	   qtype   id      ip-address	local-ip	edns-subnet remote subnet
	//
	d_qname = itoa(inZoneId);
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

			//L << Logger::Notice << kBackendId << " This is the zmqbackend version "VERSION" ("__DATE__", "__TIME__") reporting" << endl;
		}
};

static ZMQLoader zmqbackend;

