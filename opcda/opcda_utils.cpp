// opcda_utils.cpp
#ifndef OPCDA_UTILS_H
#define OPCDA_UTILS_H

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
#include "opcda_utils.h"
#include "result_formatter.hpp"

namespace OPCDA::UTILS
{
  wstring access_to_str( DWORD rights )
  {
    wstring s;

    if ( rights & OPC_READABLE )
    {
      s += L"R";
    }

    if ( rights & OPC_WRITEABLE )
    {
      s += L"W";
    }

    if ( s.empty() )
    {
      s = L"None";
    }

    return s;
  }

  wstring quality_to_str( WORD quality )
  {
    wstring q_str;

    switch ( quality & OPC_QUALITY_MASK )
    {
      case OPC_QUALITY_BAD:
        q_str = L"BAD";
        break;

      case OPC_QUALITY_UNCERTAIN:
        q_str = L"UNCERTAIN";
        break;

      case OPC_QUALITY_GOOD:
        q_str = L"GOOD";
        break;

      default:
        q_str = L"UNKNOWN";
        break;
    }

    if ( quality == ( OPC_QUALITY_BAD | OPC_QUALITY_WAITING_FOR_INITIAL_DATA ) )
    {
      q_str += L" (WAITING FOR INITIAL)";
    }

    return q_str;
  }

  string wstr_to_str( const wstring& wstr )
  {
    if ( wstr.empty() )
    {
      return string();
    }


    int size_needed = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), static_cast<int>( wstr.length() ), nullptr, 0, nullptr, nullptr );


    if ( size_needed <= 0 || size_needed > DEFAULT_MAX_STRING_BUFFER )
    {
      cerr << "Warning: String conversion buffer size issue: " << size_needed << endl;
      size_needed = min( size_needed, static_cast<int>( DEFAULT_MAX_STRING_BUFFER ) );
      if ( size_needed <= 0 )
        return string();
    }


    string result( size_needed, 0 );


    WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), static_cast<int>( wstr.length() ), &result[0], size_needed, nullptr, nullptr );

    return result;
  }

  string vartype_to_str( VARTYPE& type )
  {
    switch ( type )
    {
      case VT_EMPTY:
        return "VT_EMPTY";
      case VT_NULL:
        return "VT_NULL";
      case VT_I2:
        return "VT_I2";
      case VT_I4:
        return "VT_I4";
      case VT_R4:
        return "VT_R4";
      case VT_R8:
        return "VT_R8";
      case VT_CY:
        return "VT_CY";
      case VT_DATE:
        return "VT_DATE";
      case VT_BSTR:
        return "VT_BSTR";
      case VT_DISPATCH:
        return "VT_DISPATCH";
      case VT_ERROR:
        return "VT_ERROR";
      case VT_BOOL:
        return "VT_BOOL";
      case VT_VARIANT:
        return "VT_VARIANT";
      case VT_UNKNOWN:
        return "VT_UNKNOWN";
      case VT_DECIMAL:
        return "VT_DECIMAL";
      case VT_I1:
        return "VT_I1";
      case VT_UI1:
        return "VT_UI1";
      case VT_UI2:
        return "VT_UI2";
      case VT_UI4:
        return "VT_UI4";
      case VT_I8:
        return "VT_I8";
      case VT_UI8:
        return "VT_UI8";
      case VT_INT:
        return "VT_INT";
      case VT_UINT:
        return "VT_UINT";
      case VT_VOID:
        return "VT_VOID";
      case VT_HRESULT:
        return "VT_HRESULT";
      case VT_PTR:
        return "VT_PTR";
      case VT_SAFEARRAY:
        return "VT_SAFEARRAY";
      case VT_CARRAY:
        return "VT_CARRAY";
      case VT_USERDEFINED:
        return "VT_USERDEFINED";
      case VT_LPSTR:
        return "VT_LPSTR";
      case VT_LPWSTR:
        return "VT_LPWSTR";
      case VT_RECORD:
        return "VT_RECORD";
      case VT_INT_PTR:
        return "VT_INT_PTR";
      case VT_UINT_PTR:
        return "VT_UINT_PTR";
      case VT_FILETIME:
        return "VT_FILETIME";
      case VT_BLOB:
        return "VT_BLOB";
      case VT_STREAM:
        return "VT_STREAM";
      case VT_STORAGE:
        return "VT_STORAGE";
      case VT_STREAMED_OBJECT:
        return "VT_STREAMED_OBJECT";
      case VT_STORED_OBJECT:
        return "VT_STORED_OBJECT";
      case VT_BLOB_OBJECT:
        return "VT_BLOB_OBJECT";
      case VT_CF:
        return "VT_CF";
      case VT_CLSID:
        return "VT_CLSID";
      case VT_VERSIONED_STREAM:
        return "VT_VERSIONED_STREAM";
      case VT_BSTR_BLOB: // VT_TYPEMASK, VT_ILLEGALMASKED
        return "VT_BSTR_BLOB|VT_TYPEMASK|VT_ILLEGALMASKED";
      case VT_VECTOR:
        return "VT_VECTOR";
      case VT_ARRAY:
        return "VT_ARRAY";
      case VT_BYREF:
        return "VT_BYREF";
      case VT_RESERVED:
        return "VT_RESERVED";
      case VT_ILLEGAL:
        return "VT_ILLEGAL";
      default:
        return "";
    }
  }

  string variant_to_str( VARIANT& va )
  {
    switch ( V_VT( &va ) )
    {
      case VT_BSTR:
        return string( _bstr_t( V_BSTR( &va ) ) );

      case VT_BOOL:
        return V_BOOL( &va ) ? "TRUE" : "FALSE";

      case VT_EMPTY:
        return "";

      case VT_NULL:
        return "NULL";

      case VT_I4:
        return std::to_string( V_I4( &va ) );
      case VT_R8:
        return std::to_string( V_R8( &va ) );
      case VT_UI1:
        return std::to_string( V_UI1( &va ) );
      case VT_I1:
        return std::to_string( V_I1( &va ) );
      case VT_UI2:
        return std::to_string( V_UI2( &va ) );
      case VT_I2:
        return std::to_string( V_I2( &va ) );
      case VT_UI4:
        return std::to_string( V_UI4( &va ) );
      case VT_INT:
        return std::to_string( V_INT( &va ) );
      case VT_UINT:
        return std::to_string( V_UINT( &va ) );
      case VT_R4:
        return std::to_string( V_R4( &va ) );

      case VT_DATE:
      {
        SYSTEMTIME st;
        VariantTimeToSystemTime( V_DATE( &va ), &st );
        ostringstream oss;
        oss << st.wYear << "-" << setw( 2 ) << setfill( '0' ) << st.wMonth << "-" << setw( 2 ) << setfill( '0' ) << st.wDay << " " << setw( 2 ) << setfill( '0' ) << st.wHour << ":" << setw( 2 ) << setfill( '0' ) << st.wMinute << ":" << setw( 2 ) << setfill( '0' ) << st.wSecond;
        return oss.str();
      }

      case VT_CY:
      {
        ostringstream oss;
        oss << V_CY( &va ).int64 / 10000.0;
        return oss.str();
      }

      case VT_DECIMAL:
      {
        DECIMAL dec = V_DECIMAL( &va );
        ostringstream oss;
        oss << dec.Lo32 + ( dec.Lo64 / 1000000000.0 );
        return oss.str();
      }

      case VT_ARRAY:
      case VT_ARRAY | VT_I1:
      case VT_ARRAY | VT_I2:
      case VT_ARRAY | VT_UI2:
      case VT_ARRAY | VT_I4:
      case VT_ARRAY | VT_UI4:
      case VT_ARRAY | VT_R4:
      case VT_ARRAY | VT_R8:
      case VT_ARRAY | VT_BOOL:
      case VT_ARRAY | VT_BSTR:
      case VT_ARRAY | VT_VARIANT:
      case VT_ARRAY | VT_UI1:
      {
        SAFEARRAY* psa = V_ARRAY( &va );
        if ( psa )
        {
          long start_bount, end_bound;

          SafeArrayGetLBound( psa, 1, &start_bount );
          SafeArrayGetUBound( psa, 1, &end_bound );

          ostringstream stream;
          stream << "[";

          for ( long i = start_bount; i <= end_bound; ++i )
          {
            VARIANT element;
            VariantInit( &element );

            if ( i > start_bount )
            {
              stream << ", ";
            }

            HRESULT hr = SafeArrayGetElement( psa, &i, &element );
            if ( SUCCEEDED( hr ) )
            {
              stream << variant_to_str( element );
            }
            else
            {
              stream << "ERROR";
            }

            VariantClear( &element );
          }

          stream << "]";
          return stream.str();
        }

        return "[]";
      }
      case VT_I8:
      {
        ostringstream oss;
        oss << V_I8( &va );
        return oss.str();
      }

      case VT_UI8:
      {
        ULONGLONG value = V_UI8( &va );
        ostringstream oss;
        oss << value;
        return oss.str();
      }
      break;


      case VT_CLSID:
      {
        LPOLESTR clsid_str = nullptr;
        StringFromCLSID( *reinterpret_cast<CLSID*>( V_UNKNOWN( &va ) ), &clsid_str );
        string result = clsid_str ? string( _bstr_t( clsid_str ) ) : "";

        CoTaskMemFree( clsid_str );

        return result;
      }

      case VT_LPSTR:
        return V_BYREF( &va ) ? string( reinterpret_cast<char*>( V_BYREF( &va ) ) ) : "";

      case VT_LPWSTR:
        return V_BSTR( &va ) ? wstr_to_str( V_BSTR( &va ) ) : "";

      case VT_DISPATCH:
        return "IDispatch pointer";

      case VT_UNKNOWN:
        return "IUnknown pointer";

      case VT_BYREF | VT_I4:
        return std::to_string( *V_I4REF( &va ) );

      case VT_BYREF | VT_R8:
        return std::to_string( *V_R8REF( &va ) );

      case VT_BYREF | VT_BSTR:
        return V_BSTRREF( &va ) ? string( _bstr_t( *V_BSTRREF( &va ) ) ) : "";

      case VT_BYREF | VT_BOOL:
        return *V_BOOLREF( &va ) ? "TRUE" : "FALSE";

      case VT_BYREF:
        return "*ByRef";

      default:
        return "";
    }
  }

  wstring str_to_wstr( const string& str )
  {
    if ( str.empty() )
    {
      return wstring();
    }


    int size_needed = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), static_cast<int>( str.length() ), nullptr, 0 );


    if ( size_needed <= 0 || size_needed > DEFAULT_MAX_STRING_BUFFER )
    {
      cerr << "Warning: String conversion buffer size issue: " << size_needed << endl;
      size_needed = min( size_needed, static_cast<int>( DEFAULT_MAX_STRING_BUFFER ) );
      if ( size_needed <= 0 )
        return wstring();
    }


    wstring result( size_needed, 0 );


    MultiByteToWideChar( CP_UTF8, 0, str.c_str(), static_cast<int>( str.length() ), &result[0], size_needed );

    return result;
  }

  wstring str_to_wstr( const string& str, size_t max_buffer_size )
  {
    if ( str.empty() )
    {
      return wstring();
    }


    int size_needed = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), static_cast<int>( str.length() ), nullptr, 0 );


    if ( size_needed <= 0 || size_needed > static_cast<int>( max_buffer_size ) )
    {
      cerr << "Warning: String conversion buffer size issue: " << size_needed << endl;
      size_needed = min( size_needed, static_cast<int>( max_buffer_size ) );
      if ( size_needed <= 0 )
        return wstring();
    }


    wstring result( size_needed, 0 );


    MultiByteToWideChar( CP_UTF8, 0, str.c_str(), static_cast<int>( str.length() ), &result[0], size_needed );

    return result;
  }

  long long filetime_to_epochtime( const FILETIME& ft )
  {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    if ( uli.QuadPart < 116444736000000000ULL )
    {
      return 0;
    }

    return ( uli.QuadPart - 116444736000000000ULL ) / 10000;
  }

  string filetime_to_isotime( const FILETIME& st )
  {
  }

  long long systemtime_to_epochtime( const SYSTEMTIME& st )
  {
    FILETIME ft;
    SystemTimeToFileTime( &st, &ft );
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return static_cast<long long>( ( uli.QuadPart - 116444736000000000ULL ) / 10000 );
  }

  string systemtime_to_isotime( const SYSTEMTIME& st )
  {
    std::ostringstream oss;
    oss << std::setfill( '0' ) << std::setw( 4 ) << st.wYear << "-" << std::setw( 2 ) << st.wMonth << "-" << std::setw( 2 ) << st.wDay << "T" << std::setw( 2 ) << st.wHour << ":" << std::setw( 2 ) << st.wMinute << ":" << std::setw( 2 ) << st.wSecond << "." << std::setw( 3 ) << st.wMilliseconds << "Z";
    return oss.str();
  }

  int dword_to_int( const DWORD& value )
  {
    return static_cast<int>( value );
  }

  int word_to_int( const WORD& value )
  {
    return static_cast<int>( value );
  }


  /**
   * @brief Converts HRESULT, VARIANT, VARTYPE, SYSTEMTIME, FILETIME types to corresponding string value.
   *
   * @param v
   * @return string
   */
  string to_str( const std::variant<HRESULT, wstring, VARIANT, VARTYPE, SYSTEMTIME, FILETIME>& v )
  {
    try
    {
      /* HRESULT
       */
      if ( std::holds_alternative<HRESULT>( v ) )
      {
        HRESULT hr = std::get<HRESULT>( v );
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << hr;
        return oss.str();
      }
      /* wstring
       */
      else if ( std::holds_alternative<wstring>( v ) )
      {
        wstring wstr = std::get<wstring>( v );

        if ( wstr.empty() )
        {
          return string();
        }

        int size_needed = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), static_cast<int>( wstr.length() ), nullptr, 0, nullptr, nullptr );
        size_needed = std::min( size_needed, static_cast<int>( DEFAULT_MAX_STRING_BUFFER ) );

        if ( size_needed <= 0 )
        {
          return string();
        }

        string result( size_needed, 0 );
        WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), static_cast<int>( wstr.length() ), &result[0], size_needed, nullptr, nullptr );
        return result;
      }
      /* VARIANT
       */
      else if ( std::holds_alternative<VARIANT>( v ) )
      {
        VARIANT va = std::get<VARIANT>( v );
        return variant_to_str( va );
      }
      /* VARTYPE
       */
      else if ( std::holds_alternative<VARTYPE>( v ) )
      {
        VARTYPE vt = std::get<VARTYPE>( v );
        return vartype_to_str( vt );
      }
      /* SYSTEMTIME
       */
      else if ( std::holds_alternative<SYSTEMTIME>( v ) )
      {
        SYSTEMTIME st = std::get<SYSTEMTIME>( v );
        return systemtime_to_isotime( st );
      }
      /* FILETIME
       */
      else if ( std::holds_alternative<FILETIME>( v ) )
      {
        FILETIME ft = std::get<FILETIME>( v );
        return filetime_to_isotime( ft );
      }
    }
    catch ( ... )
    {
    }

    return "";
  }
} // namespace OPCDA::UTILS

#endif