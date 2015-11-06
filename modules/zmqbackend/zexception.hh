#ifndef ZEXCEPTION_HH
#define ZEXCEPTION_HH
/* (C) 2002 POWERDNS.COM BV */

#include<string>

class ZException
{
	public:
		ZException()
		{
			reason = "-";
		};

		ZException(string r)
		{
			reason = r;
		};

		const string what()
		{
			return reason;
		};

		std::string reason;
};

#endif
