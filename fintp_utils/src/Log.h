/*
* FinTP - Financial Transactions Processing Application
* Copyright (C) 2013 Business Information Systems (Allevo) S.R.L.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>
* or contact Allevo at : 031281 Bucuresti, 23C Calea Vitan, Romania,
* phone +40212554577, office@allevo.ro <mailto:office@allevo.ro>, www.allevo.ro.
*/

#ifndef FINTPLOG_H
#define FINTPLOG_H

#include <string>
#include <iostream>

using namespace std;

#ifdef WIN32
	#define FINTPFILE ( ( strrchr(__FILE__, '\\') != NULL ) ? strrchr(__FILE__, '\\') + 1 : __FILE__ )
#else 
	#define FINTPFILE (__FILE__)
#endif

#ifdef DEBUG_LOG_ENABLED
	#define	DEBUG_LOG( expr ) { cout << "DEBUG_LOG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; }
#else 
	#define DEBUG_LOG( expr ) ;
#endif 

#define TRACE_LOG( expr ) \
	{ \
		try \
		{ \
			cerr << "TRACE_LOG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; \
		} catch( ... ){}; \
	}
#endif //FINTPLOG_H
