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
#ifndef X509_H
#define X509_H

#include <string>
#include "../DllMain.h"
#include <sstream>
#include <iostream>

#include "Trace.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace FinTP
{
	//TODO: Upload certificate in client and initialize X509Parser passing string certificate
	class ExportedObject X509Parser
	{
		private:

			string m_SerialDEC;
			string m_Issuer;
			string m_Subject;
			string m_PublicKeyType;

			X509* m_X509C;
			string ReadName( X509_NAME *subj_or_issuer );
			void ReadSerialNumber( ASN1_INTEGER *bs );
			string ReadKeyType();
			
			/**
			* Other implemented but not used methods
			void UploadCertificate( const string& certificateFileName );
			string ReadASN1ISODateTime( const ASN1_TIME *tm );
			string pem( X509* x509 );
			void ASN1DateTimeParse( const ASN1_TIME *time, int& year, int& month, int& day, int& hour, int& minute, int& second );
			void ReadThumbPrint( X509* x509 );
			void ReadKeySize( X509* x509 );
			*/

		public:

			/**
			\brief \return set class members with signature info
			\certificateFileName - Certificate path
			*/
			X509Parser( const string& certificateFileName );
			~X509Parser(){};
		
			string getSerialNumber() const { return m_SerialDEC; };
			string getIssuer() const { return m_Issuer; };
			string getSubject() const { return m_Subject; };
			string getPublicKeyType() const { return m_PublicKeyType; };
		
			/**
			* Other implemented but not used fields

			static string m_ThumbPrint;
			static string m_Version;
			static string m_SerialHEX;
			static string m_SignatureAlgorithm;	
			static string m_PublicKeyEcCurveName;
			static int m_PublicKeySize;
			static string m_AfterASN1ISODateTime;
			static string m_BeforeASN1ISODateTime;
			*/
	};
}
#endif