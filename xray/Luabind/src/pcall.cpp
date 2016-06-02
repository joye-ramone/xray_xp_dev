// Copyright (c) 2003 Daniel Wallin and Arvid Norberg

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

#include <luabind/detail/pcall.hpp>
#include <luabind/error.hpp>
#include <luabind/lua_include.hpp>

xr_map <lua_State*, xrCriticalSection*> g_mutex_map;


namespace luabind { namespace detail
{
	int pcall(lua_State *L, int nargs, int nresults)
	{
		pcall_callback_fun e = get_pcall_callback();
		int en = 0;
		if ( e )
		{
			int base = lua_gettop(L) - nargs;
			lua_pushcfunction(L, e);
			lua_insert(L, base);  // push pcall_callback under chunk and args
			en = base;
  		}
		xrCriticalSection *cs = g_mutex_map[L];
		if (!cs)
		{
			cs = xr_new<xrCriticalSection>();
			g_mutex_map[L] = cs;
		}				
		
		BOOL sync = FALSE;
		for (int i = 0; i < 1000; i++)
		{
			sync = cs->TryEnter();
			if (sync) break;
			Sleep(1);
		}
		R_ASSERT3(sync, "cannot lock MT-access lua_State", cs->Dump());	
		int result = -1;
		__try
		{
			result = lua_pcall(L, nargs, nresults, en);			
		}
		__finally
		{
			if (en) lua_remove(L, en);  // remove pcall_callback
			cs->Leave();		
		}

		return result;
	}

	int resume_impl(lua_State *L, int nargs, int)
	{
		return lua_resume(L, nargs);
	}

}}
