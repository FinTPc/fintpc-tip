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
#ifndef ECDSASIGN_H
#define ECDSASIGN_H

#include <string>
#include <iostream>
#include <openssl/ec.h>      // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>   // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h> // for NID_secp192k1
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/bn.h>

#include "../DllMain.h"
#include "Trace.h"
#include "AbstractFilter.h"
#include "SSL/HMAC.h"
#include "Base64.h"
#include "StringUtil.h"

namespace FinTP
{

	class ExportedObject ECDSASign
	{
		private:

			static EC_KEY* m_PrivateECKey;
			static EC_KEY* m_PublicECKey;
			//static void CreateKeyPairEC_KEY( string pemPrivateKey );
			static void ReadPrivateEC_KEY( string key );
			static void ReadPublicEC_KEY( string key );
			static string GetBase64Signature( BIGNUM* r_sign, BIGNUM* s_sign );

		public:

			/**Sign message with EC Private Key
			/param 1 - Private key. Is given as string and format as PEM
			/param 2 - Plain Text. Is given as it is*/
			static string SignMessage( string pemKeyFile, string plainText = "" );

			/**Validate message signature using EC Public Key
			/param 1 - Public key. Is given as string and format as PEM
			/param 2 - Plain Text. Is given as it is
			/param 3 - Signature. Is given as BASE64 encoding format*/
			static bool VerifySignature( const string pemKeyFile, const string plainText, const string signatureBase64 );

			~ECDSASign();
	};

}
#endif