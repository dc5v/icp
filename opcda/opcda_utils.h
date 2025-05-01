// opcda_client.cpp
#define NOMINMAX
#include <Lmcons.h>
#include <algorithm>
#include <atlbase.h>
#include <chrono>
#include <comutil.h>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <opccomn.h>
#include <opcda.h>
#include <sstream>
#include <string>
#include <windows.h>

#include "logger.h"
#include "opcda_client.h"
#include "result_formatter.hpp"

namespace OPCDA::UTILS
{
  int dword_to_int( const DWORD& value );
  int word_to_int( const WORD& value );
  long long filetime_to_epochtime( const FILETIME& ft );
  long long systemtime_to_epochtime( const SYSTEMTIME& st );
  string filetime_to_isotime( const FILETIME& st );
  string systemtime_to_isotime( const SYSTEMTIME& st );
  string to_str( const std::variant<HRESULT, wstring, VARIANT, VARTYPE, SYSTEMTIME, FILETIME>& v );
  string variant_to_str( VARIANT& va );
  string vartype_to_str( VARTYPE& type );
  string wstr_to_str( const wstring& wstr );
  wstring access_to_str( DWORD rights );
  wstring quality_to_str( WORD quality );
  wstring str_to_wstr( const string& str );
  wstring str_to_wstr( const string& str, size_t max_buffer_size );

} // namespace OPCDA::UTILS