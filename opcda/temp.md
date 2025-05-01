#include "opcda_client.h"
#include <Lmcons.h>
#include <algorithm>
#include <chrono>
#include <comutil.h>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <opccomn_i.c>
#include <opcda_i.c>
#include <opcenum_i.c>
#include <sstream>
#include <string>
#include <windows.h>


#ifndef CLSID_OPCEnum
// {13486D51-4821-11D2-A494-3CB306C10000}
static const CLSID CLSID_OPCEnum = { 0x13486D51, 0x4821, 0x11D2, { 0xA4, 0x94, 0x3C, 0xB3, 0x06, 0xC1, 0x00, 0x00 } };
#endif

#ifndef CATID_OPCDAServer10
// {"63D5F430-CFE4-11d1-B2C8-0060083BA1FB"}
const CATID CATID_OPCDAServer10 = { 0x63D5F430, 0xCFE4, 0x11d1, { 0xB2, 0xC8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
#endif

#ifndef CATID_OPCDAServer20
// {63D5F432-CFE4-11d1-B2C8-0060083BA1FB}
const CATID CATID_OPCDAServer20 = { 0x63D5F432, 0xCFE4, 0x11d1, { 0xB2, 0xC8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
#endif

#ifndef CATID_OPCDAServer30
// {CC603642-66D7-48f1-B69A-B625E73652D7}
const CATID CATID_OPCDAServer30 = { 0xCC603642, 0x66D7, 0x48f1, { 0xB6, 0x9A, 0xB6, 0x25, 0xE7, 0x36, 0x52, 0xD7 } };
#endif

OpcDaClient::OpcDaClient() : is_com_init( false ), m_group_handle_server( 0 )
{
}

OpcDaClient::~OpcDaClient()
{
  disconnect();
  com_free();
}

void OpcDaClient::debug( const std::string& tag, HRESULT& hr, const std::string& message )
{
  if ( FAILED( hr ) )
  {
    _com_error err( hr );
    std::cout << "[" << tag << "] ERROR: " << message << " HRESULT: 0x" << std::hex << hr << ": " << err.ErrorMessage() << "\n" << err.Description() << std::endl;
  }
  else
  {
    std::cout << "[" << tag << "] " << message << " SUCCEEDED. HRESULT: 0x" << std::hex << hr << std::endl;
  }
}

void OpcDaClient::debug( const std::string& tag, const std::string& message )
{
  std::cout << "[" << tag << "] " << message << std::endl;
}

void OpcDaClient::debug( const std::string& tag, const std::exception& message )
{
  std::cout << "[" << tag << "] ERROR: Exception occurred: " << message.what() << std::endl;
}

void OpcDaClient::debug( const std::string& message )
{
  std::cout << "DEBUG: " << message << std::endl;
}

bool OpcDaClient::com_init()
{
  if ( is_com_init )
  {
    return true; // return S_OK;
  }

  HRESULT hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );

  if ( SUCCEEDED( hr ) )
  {
    hr = CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IDENTIFY, NULL, EOAC_NONE, NULL );

    if ( SUCCEEDED( hr ) )
    {
      is_com_init = true;
      return true;
    }

    CoUninitialize();
  }

  return false;
}

void OpcDaClient::com_free()
{
  if ( is_com_init )
  {
    CoUninitialize();
    is_com_init = false;
  }
}

static std::string generate_groupname()
{
  char computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
  DWORD size = sizeof( computerName );
  GetComputerNameA( computerName, &size );

  char userName[UNLEN + 1] = { 0 };
  size = sizeof( userName );
  GetUserNameA( userName, &size );

  auto now = std::chrono::system_clock::now();
  time_t now_c = std::chrono::system_clock::to_time_t( now );
  struct tm t;
  localtime_s( &t, &now_c );

  std::ostringstream oss_time;
  oss_time << std::setfill( '0' ) << std::setw( 4 ) << ( t.tm_year + 1900 ) << std::setw( 2 ) << ( t.tm_mon + 1 ) << std::setw( 2 ) << t.tm_mday << std::setw( 2 ) << t.tm_hour << std::setw( 2 ) << t.tm_min << std::setw( 2 ) << t.tm_sec;

  std::string unique = std::string( computerName ) + "-" + std::string( userName ) + "+" + oss_time.str();

  std::hash<std::string> hasher;
  size_t hash_value = hasher( unique );

  std::ostringstream oss_hash;
  oss_hash << std::hex << std::setw( sizeof( size_t ) * 2 ) << std::setfill( '0' ) << hash_value;

  std::string group_name = "GROUP_" + oss_hash.str();
  std::transform( group_name.begin(), group_name.end(), group_name.begin(), ::toupper );

  return group_name;
}

std::string OpcDaClient::wstr_to_str( const std::wstring& wstr )
{
  std::string result;
  result.reserve( wstr.length() );

  std::locale loc;
  for ( wchar_t c : wstr )
  {
    result += std::use_facet<std::ctype<wchar_t>>( loc ).narrow( c, '?' );
  }

  return result;
}

std::wstring OpcDaClient::str_to_wstr( const std::string& str )
{
  if ( str.empty() )
  {
    return std::wstring();
  }

  int size_needed = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, nullptr, 0 );

  std::wstring wstr( size_needed - 1, 0 );
  MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed );

  return wstr;
}

LPWSTR OpcDaClient::str_to_lpw_str( const std::string& str )
{
  return const_cast<LPWSTR>( str_to_wstr( str ).c_str() );
}

long long OpcDaClient::filetime_to_epochtime( const FILETIME& ft )
{
  return ( ( ULARGE_INTEGER{ ft.dwLowDateTime, ft.dwHighDateTime } ).QuadPart - 116444736000000000ULL ) / 10000;
}

int OpcDaClient::dword_to_int( const DWORD& value )
{
  return static_cast<int>( value );
}

int OpcDaClient::word_to_int( const WORD& value )
{
  return static_cast<int>( value );
}

std::vector<OPCDA_CONNECT_INFO> OpcDaClient::discovery( const std::string& host )
{
  std::wstring whost = str_to_wstr( host );
  std::vector<OPCDA_CONNECT_INFO> servers;

  if ( !is_com_init )
  {
    return {};
  }

  CComPtr<IOPCServerList> server_list;
  CComPtr<IEnumCLSID> enum_clsid;

  CATID da_categories[3] = { CATID_OPCDAServer30, CATID_OPCDAServer20, CATID_OPCDAServer10 };
  COSERVERINFO info = { 0 };
  MULTI_QI mq[1];

  info.pwszName = whost.empty() ? NULL : const_cast<LPWSTR>( whost.c_str() );
  mq[0] = { &IID_IOPCServerList, NULL, S_OK };

  HRESULT hr = CoCreateInstanceEx( CLSID_OPCEnum, NULL, CLSCTX_REMOTE_SERVER | CLSCTX_LOCAL_SERVER, ( whost.empty() ? NULL : &info ), 1, mq );
  if ( FAILED( hr ) || FAILED( mq[0].hr ) )
  {
    hr = CoCreateInstanceEx( CLSID_OPCEnum, NULL, CLSCTX_INPROC_SERVER, NULL, 1, mq );
    if ( FAILED( hr ) || FAILED( mq[0].hr ) )
    {
      return {};
    }
  }

  server_list = ( IOPCServerList* )mq[0].pItf;
  hr = server_list->EnumClassesOfCategories( 3, da_categories, 0, NULL, &enum_clsid );
  if ( FAILED( hr ) )
  {
    return {};
  }

  CLSID server_clsid;
  ULONG fetched;
  while ( ( hr = enum_clsid->Next( 1, &server_clsid, &fetched ) ) == S_OK && fetched == 1 )
  {
    LPOLESTR prog_id_ole = nullptr;
    LPOLESTR user_type_ole = nullptr;
    HRESULT details_hr = server_list->GetClassDetails( server_clsid, &prog_id_ole, &user_type_ole );
    OPCDA_CONNECT_INFO info;

    if ( SUCCEEDED( details_hr ) )
    {
      info.available = true;
      info.clsid = server_clsid;
      info.prog_id = ( prog_id_ole != nullptr ) ? _bstr_t( prog_id_ole ) : "";
      info.description = ( user_type_ole != nullptr ) ? _bstr_t( user_type_ole ) : L"";

      CoTaskMemFree( prog_id_ole );
      CoTaskMemFree( user_type_ole );
    }
    else
    {
      std::ostringstream oss;
      oss << "0x" << std::hex << details_hr;

      info.available = false;
      info.error_code = oss.str();
    }

    servers.push_back( info );
  }

  if ( FAILED( hr ) && hr != S_FALSE )
  {
    return {};
  }

  return servers;
}

bool OpcDaClient::connect( OPCDA_CONNECT_INFO& info )
{
  std::string host = info.host.empty() ? "localhost" : info.host;

  if ( !IsEqualCLSID( info.clsid, CLSID_NULL ) )
  {
    HRESULT hr = connect_clsid( host, info.clsid );
    if ( SUCCEEDED( hr ) )
    {
      return true;
    }
  }

  if ( !info.prog_id.empty() )
  {
    HRESULT hr = connect_progid( host, info.prog_id );

    if ( SUCCEEDED( hr ) )
    {
      return true;
    }
  }

  return false;
}

bool OpcDaClient::connect_progid( const std::string& host_name, const std::string& progid )
{
  CLSID clsid;
  HRESULT hr = CLSIDFromProgID( str_to_lpw_str( progid ), &clsid );

  if ( FAILED( hr ) )
  {
    debug( "connect_progid", hr );
    return false;
  }

  return connect_clsid( host_name, clsid );
}

bool OpcDaClient::connect_clsid( const std::string& host_name, const CLSID& server_clsid )
{
  disconnect();

  m_default_group = generate_groupname();

  if ( !is_com_init )
  {
    return false;
  }

  COSERVERINFO server_info = { 0 };
  server_info.pwszName = host_name.empty() ? NULL : str_to_lpw_str( host_name );

  MULTI_QI mq[1];
  mq[0].pIID = &IID_IOPCServer;
  mq[0].pItf = NULL;
  mq[0].hr = S_OK;

  HRESULT hr = CoCreateInstanceEx( server_clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, ( host_name.empty() ? NULL : &server_info ), 1, mq );
  if ( FAILED( hr ) || FAILED( mq[0].hr ) )
  {
    debug( "connect_clsid 1", hr );

    hr = CoCreateInstanceEx( server_clsid, NULL, CLSCTX_INPROC_SERVER, NULL, 1, mq );
    if ( FAILED( hr ) || FAILED( mq[0].hr ) )
    {
      debug( "connect_clsid 2", hr );
      return false;
    }
  }

  /* m_server = ( IOPCServer* )mq[0].pItf;
  hr = m_server->QueryInterface( IID_IOPCBrowseServerAddressSpace, ( void** )&browser );
  if ( FAILED( hr ) )
  {
    hr = m_server->QueryInterface( IID_IOPCItemProperties, ( void** )&m_item_properties );
    if ( FAILED( hr ) )
    {
      return false;
    }
  }

  hr = add_opc_group( str_to_wstr( m_default_group ) );
  if ( FAILED( hr ) )
  {
    disconnect();
    return false;
    // std::cerr << "Error: Failed to add OPC group. HRESULT: 0x" << std::hex << hr << std::endl;
  } */

  return true;
}

void OpcDaClient::disconnect()
{
  remove_opc_group();

  if ( browser )
  {
    browser.Release();
  }

  if ( m_item_properties )
  {
    m_item_properties.Release();
  }

  if ( m_server )
  {
    m_server.Release();
  }
}


OPCDA_SERVER_INFO OpcDaClient::get_server_status()
{
  OPCDA_SERVER_INFO info;

  auto server_state_to_string = []( DWORD state ) -> std::string
  {
    switch ( state )
    {
      case OPC_STATUS_RUNNING:
        return "OPC_STATUS_RUNNING";
      case OPC_STATUS_FAILED:
        return "OPC_STATUS_FAILED";
      case OPC_STATUS_NOCONFIG:
        return "OPC_STATUS_NOCONFIG";
      case OPC_STATUS_SUSPENDED:
        return "OPC_STATUS_SUSPENDED";
      case OPC_STATUS_TEST:
        return "OPC_STATUS_TEST";
      case OPC_STATUS_COMM_FAULT:
        return "OPC_STATUS_COMM_FAULT";
      default:
        return "UNKNOWN_" + std::to_string( state );
    }
  };

  if ( m_server )
  {
    OPCSERVERSTATUS* server_status_ptr = nullptr;
    HRESULT hr = m_server->GetStatus( &server_status_ptr );

    if ( SUCCEEDED( hr ) && server_status_ptr )
    {
      info.server_started_epochtime = filetime_to_epochtime( server_status_ptr->ftStartTime );
      info.status_created_epochtime = filetime_to_epochtime( server_status_ptr->ftCurrentTime );
      info.status_updated_epochtime = filetime_to_epochtime( server_status_ptr->ftLastUpdateTime );
      info.status = server_status_ptr->dwServerState;
      info.status_string = server_state_to_string( server_status_ptr->dwServerState );
      info.enabled_group_len = dword_to_int( server_status_ptr->dwGroupCount );
      info.major_version = word_to_int( server_status_ptr->wMajorVersion );
      info.minor_version = word_to_int( server_status_ptr->wMinorVersion );
      info.build_version = word_to_int( server_status_ptr->wBuildNumber );
      info.vendor = wstr_to_str( std::wstring( server_status_ptr->szVendorInfo ) );

      CoTaskMemFree( server_status_ptr->szVendorInfo );
      CoTaskMemFree( server_status_ptr );
    }
  }

  return info;
}
