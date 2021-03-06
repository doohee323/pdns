% DNSDIST(1)
% PowerDNS.com BV
% 2013

# NAME
**dnsdist** - tool to balance DNS queries over downstream servers

# SYNOPSIS
dnsdist [*OPTION*]... *ADDRESS*...

# DESCRIPTION
**dnsdist** receives DNS queries and relays them to one or more downstream
servers. It subsequently sends back responses to the original requestor.

dnsdist operates over TCP and UDP, and strives to deliver very high
performance over both.

Currently, queries are sent to the downstream server with the least
outstanding queries. This effectively implies load balancing, making sure
that slower servers get less queries.

If a reply has not come in after a few seconds, it is removed from the
queue, but in the short term, timeouts do cause a server to get less
traffic.

IPv4 and IPv6 operation can be mixed and matched, in other words, queries
coming in over IPv6 could be forwarded to IPv4 and vice versa.

# SCOPE
dnsdist does not 'think' about DNS, and does not perform any kind of
caching, nor is it aware of the quality of the answers it is relaying.

dnsdist assumes that each query leads to exactly one response, which is true
for all DNS except for AXFR, which is therefore not supported.

The goal for dnsdist is to remain simple. If more powerful loadbalancing is
required, dedicated hardware or software is recommended. Linux Virtual
Server for example is often mentioned.

# OPTIONS
--help
:    Show a brief summary of the options.

--verbose
:    Be wordy on what the program is doing

--local *ADDRESS*
:    Bind to ADDRESS, Supply as many addresses (using multiple **--local**
     statements) to listen on as required. Specify IPv4 as 0.0.0.0:53 and IPv6
     as [::]:53.

--daemon
:    Daemonize and run in the background

Finally, supply as many downstream addresses as required. Remote port defaults
to 53.

# BUGS
Right now, the TCP support has some rather arbitrary limits.

# RESOURCES
Website: http://www.powerdns.com
