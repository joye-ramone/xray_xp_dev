
//  (C) Copyright Boost.org 1999. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

// Test file for macro BOOST_NO_MS_INT64_NUMERIC_LIMITS
// This file should compile, if it does not then
// BOOST_NO_MS_INT64_NUMERIC_LIMITS needs to be defined.
// see boost_no_i64_limits.cxx for more details

// Do not edit this file, it was generated automatically by
// ../tools/generate from boost_no_i64_limits.cxx on
// Thu Sep 12 12:24:12  2002

// Must not have BOOST_ASSERT_CONFIG set; it defeats
// the objective of this file:
#ifdef BOOST_ASSERT_CONFIG
#  undef BOOST_ASSERT_CONFIG
#endif

#include <boost/config.hpp>
#include "test.hpp"

#ifndef BOOST_NO_MS_INT64_NUMERIC_LIMITS
#include "boost_no_i64_limits.cxx"
#else
namespace boost_no_ms_int64_numeric_limits = empty_boost;
#endif

int cpp_main( int, char *[] )
{
   return boost_no_ms_int64_numeric_limits::test();
}  
   
