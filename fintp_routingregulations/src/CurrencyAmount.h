#pragma once

#include "DllMainRoutingRegulations.h"
#include "StringUtil.h"

// P1P2.Pd
class ExportedRRObject currency_amount
{
	public :

#ifdef WIN32
		typedef __int64 int_part_type;
#else
		typedef long long int_part_type;
#endif
		
		currency_amount( const string& value );

		currency_amount( const currency_amount::int_part_type intPart = 0, const unsigned int decPart = 0 ) : m_Pi( intPart ), m_Pd( decPart )
		{
		}

		const currency_amount& operator=( const currency_amount& source )
		{
			m_Pi = source.m_Pi;
			m_Pd = source.m_Pd;
			return *this;
		}

		bool operator==( const currency_amount& source )
		{
			return ( ( m_Pi == source.m_Pi ) && ( m_Pd == source.m_Pd ) );		
		}

		bool operator!=( const currency_amount& source )
		{
			return ( ( m_Pi != source.m_Pi ) || ( m_Pd != source.m_Pd ) );		
		}

		currency_amount operator+( const currency_amount& source )
		{
			div_t divResult = div( m_Pd + source.m_Pd, 100 );
			return currency_amount( m_Pi + source.m_Pi + divResult.quot, divResult.rem );
		}

		currency_amount operator-( const currency_amount& source )
		{
			int carry = m_Pd - source.m_Pd;
			if ( carry < 0 )
				return currency_amount( m_Pi - source.m_Pi - 1, 100 + carry  );
			else
				return currency_amount( m_Pi - source.m_Pi, carry );
		}

		friend ostream& operator << ( ostream& os, const currency_amount& amount );

	private : 

		currency_amount::int_part_type m_Pi;
		unsigned int m_Pd;
};
