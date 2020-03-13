#include "CurrencyAmount.h"

currency_amount::currency_amount( const string& value ) : m_Pi( 0 ), m_Pd( 0 )
{
	string actualValue = StringUtil::Replace( value, ".", "," );
	string::size_type decFinder = actualValue.find( ',' );
	if ( decFinder == string::npos )
	{
		m_Pd = 0;
#ifdef WIN32
		m_Pi = _atoi64( actualValue.c_str() );
#else
		m_Pi = strtoll( actualValue.c_str(), NULL, 10 );
#endif
	}
	else
	{
		m_Pd = atoi( actualValue.substr( decFinder + 1 ).c_str() );
#ifdef WIN32		
		m_Pi = _atoi64( actualValue.substr( 0, decFinder ).c_str() );
#else
		m_Pi = strtoll( actualValue.c_str(), NULL, 10 );
#endif
	}
}

ostream& operator << ( ostream& os, const currency_amount& item )
{
	os << item.m_Pi << "." << item.m_Pi;
	return os;
}