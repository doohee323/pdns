//
// File    : zmqbackend.hh
// Version : $Id: pipebackend.hh 1976 2011-02-06 11:11:34Z ahu $
//

#ifndef ZMQBACKEND_HH
#define ZMQBACKEND_HH

#include <string>
#include <queue>
#include <map>
#include <sys/types.h>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "pdns/namespaces.hh"

#include "zhelpers.hpp"
#include "zexception.hh"

class ZMQBackend : public DNSBackend
{
	public:
		ZMQBackend(const string &suffix = "");
		~ZMQBackend();

		void lookup(const QType&, const DNSName& qdomain, DNSPacket *p=0, int zoneId=-1);
		bool get(DNSResourceRecord &r);

		bool list(const DNSName& target, int domain_id, bool include_disabled=false);
		static DNSBackend *maker();

		//
		// special cased SOA handling, probably not implemented the way you want
		//
		bool getSOA(const DNSName& name, SOAData& soadata, DNSPacket*);

		virtual bool setDomainMetadata(const DNSName& name, const string& kind, const std::vector<std::basic_string<char> >& meta);
	private:
		void connect();
		void send(const string &line);
		void receive(string &line);

		string d_qname;
		QType d_qtype;
		zmq::socket_t*  zmq_socket;
		string zmq_url;
		int zmq_timeout;
		bool do_query_logging;
		bool do_wildcards;
		int version;
		string suffix;

		//
		// where we store the response
		//
		std::queue<std::string> lineQueue;
};

#endif
