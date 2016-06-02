#include "StdAfx.h"
#include "script_storage.h"
#include "script_engine.h"
#include "../lua_tools.h"

#pragma warning(disable: 4512)

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <stdexcept>
using std::runtime_error;

#include <cstdlib>
using std::exit;

#include "cryptopp/osrng.h"
using CryptoPP::AutoSeededRandomPool;

#include "cryptopp/eccrypto.h"
using CryptoPP::ECP;
using CryptoPP::ECDSA;

#include "cryptopp/sha.h"
using CryptoPP::SHA1;

#include "cryptopp/queue.h"
using CryptoPP::ByteQueue;

#include "cryptopp/oids.h"
using CryptoPP::OID;

// ASN1 is a namespace, not an object
#include "cryptopp/asn.h"
using namespace CryptoPP::ASN1;

#include "cryptopp/integer.h"
using CryptoPP::Integer;

#include "CryptoPP/fips140.h"

#define NLC_DEV_BUILD


#ifdef NLC_DEV_BUILD

void dump_raw_key(lua_State *L, ByteQueue &K)
{
	lua_createtable (L, 0, 0);
	int t_idx = lua_gettop (L);
	size_t max_ret = (size_t) K.MaxRetrievable();
	// cout << " private key raw size = " << max_ret << endl;
	byte *buff_pk = (byte*) lua_newuserdata (L, max_ret);
	K.Peek (buff_pk, max_ret);
	lua_pushlightuserdata (L, buff_pk);
	lua_setfield (L, t_idx, "bytes");
	lua_pushinteger (L, max_ret);
	lua_setfield (L, t_idx, "size");
}

int create_keys(lua_State *L)
{
	AutoSeededRandomPool prng;
    ByteQueue privateKey, publicKey;
	//////////////////////////////////////////////////////

    // Generate private key
    ECDSA<ECP, SHA1>::PrivateKey privKey;
    privKey.Initialize( prng, secp160r1() );
    privKey.Save( privateKey );	
    // Create public key
    ECDSA<ECP, SHA1>::PublicKey pubKey;
    privKey.MakePublicKey( pubKey );
    pubKey.Save ( publicKey );

	dump_raw_key (L, privateKey);
	dump_raw_key (L, publicKey);	
	return 2;
}


#endif

byte  *lua_tobuff(lua_State *L, int index, size_t &size)
{
	if (!lua_istable(L, index)) return NULL;

	const void *result = NULL;

	lua_getfield(L, index, "bytes");
	if (lua_islightuserdata(L, -1))
		result = lua_topointer(L, -1);
	else
		result = lua_touserdata(L, -1);
	lua_pop (L, 1);
	lua_getfield(L, index, "size");
	size = lua_tointeger(L, -1);
	lua_pop (L, 1);
	return (byte*)result;
}

int verify_ecs(lua_State *L)
{
	ByteQueue publicKey;
	byte	  *message;
	size_t	   message_sz;
	byte	  *signature;
	size_t	   signature_sz;
	byte	  *temp;
	size_t	   pubk_sz;
	
	message		= lua_tobuff(L, 1, message_sz);
	signature	= lua_tobuff(L, 2, signature_sz);
	temp		= lua_tobuff(L, 3, pubk_sz);

	if (message_sz && signature_sz && pubk_sz);
	else
	{
		lua_pushboolean(L, FALSE);
		return 1;
	}

	publicKey.Put(temp, pubk_sz);

    ECDSA<ECP, SHA1>::Verifier verifier( publicKey );

    bool result = verifier.VerifyMessage( message, message_sz, signature, signature_sz );

	lua_pushboolean(L, result);
	return 1;
}


void attach_upgrades(lua_State *L)
{

#ifdef NLC_DEV_BUILD
	lua_register(L, "create_keys",  create_keys);
#endif
	lua_register(L, "verify_ecs",  verify_ecs);
}