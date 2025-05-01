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
#include "opcda_utils.h"
#include "result_formatter.hpp"


using namespace std;

OpcDaClient::OpcDaClient() : is_com_init( false ), m_group_handle_server( 0 ), m_max_browse_depth( DEFAULT_MAX_BROWSE_DEPTH ), m_max_string_buffer( DEFAULT_MAX_STRING_BUFFER )
{
}

OpcDaClient::~OpcDaClient()
{
  disconnect();
  com_free();
}

void OpcDaClient::set_max_browse_depth( int depth )
{
  if ( depth > 0 )
  {
    m_max_browse_depth = depth;
  }
}

void OpcDaClient::debug( const string& tag, HRESULT& hr, const string& message )
{
  if ( FAILED( hr ) )
  {
    ostringstream oss;
    oss << "[" << tag << "] " << message << " FAILED. HRESULT: 0x" << hex << hr;
    Logger::instance().logError( oss.str() );
  }
  else
  {
    ostringstream oss;
    oss << "[" << tag << "] " << message << " SUCCEEDED. HRESULT: 0x" << hex << hr;
    Logger::instance().logInfo( oss.str() );
  }
}

void OpcDaClient::debug( const string& tag, const string& message )
{
  ostringstream oss;
  oss << "[" << tag << "] " << message;
  Logger::instance().logDebug( oss.str() );
}

void OpcDaClient::debug( const string& tag, const exception& message )
{
  ostringstream oss;
  oss << "[" << tag << "] ERROR: Exception occurred: " << message.what();
  Logger::instance().logError( oss.str() );
}

void OpcDaClient::debug( const string& message )
{
  Logger::instance().logDebug( "DEBUG: " + message );
}

bool OpcDaClient::com_init()
{
  if ( is_com_init )
  {
    return true;
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

string generate_groupname()
{
  char computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
  DWORD size = sizeof( computerName );
  GetComputerNameA( computerName, &size );

  char userName[UNLEN + 1] = { 0 };
  size = sizeof( userName );
  GetUserNameA( userName, &size );

  auto now = chrono::system_clock::now();
  time_t now_c = chrono::system_clock::to_time_t( now );
  struct tm t;
  localtime_s( &t, &now_c );

  ostringstream oss_time;
  oss_time << setfill( '0' ) << setw( 4 ) << ( t.tm_year + 1900 ) << setw( 2 ) << ( t.tm_mon + 1 ) << setw( 2 ) << t.tm_mday << setw( 2 ) << t.tm_hour << setw( 2 ) << t.tm_min << setw( 2 ) << t.tm_sec;

  string unique = string( computerName ) + "-" + string( userName ) + "+" + oss_time.str();

  hash<string> hasher;
  size_t hash_value = hasher( unique );

  ostringstream oss_hash;
  oss_hash << hex << setw( sizeof( size_t ) * 2 ) << setfill( '0' ) << hash_value;

  string group_name = "GROUP_" + oss_hash.str();
  transform( group_name.begin(), group_name.end(), group_name.begin(), ::toupper );

  return group_name;
}

vector<OPCDA_CONNECT_INFO> OpcDaClient::discovery( const string& host )
{
  wstring whost = OPCDA::UTILS::str_to_wstr( host );
  vector<OPCDA_CONNECT_INFO> servers;
  bool com_initialized = false;

  try
  {

    HRESULT hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    if ( FAILED( hr ) )
    {
      cerr << "Failed to initialize COM for discovery. HRESULT: 0x" << hex << hr << endl;
      return servers;
    }

    com_initialized = true;


    hr = CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IDENTIFY, NULL, EOAC_NONE, NULL );
    if ( FAILED( hr ) )
    {
      cerr << "Warning: COM security initialization failed. HRESULT: 0x" << hex << hr << endl;
    }

    CComPtr<IOPCServerList> server_list;
    CComPtr<IEnumCLSID> enum_clsid;

    CATID da_categories[3] = { CATID_OPCDAServer30, CATID_OPCDAServer20, CATID_OPCDAServer10 };
    COSERVERINFO info = { 0 };
    MULTI_QI mq[1];

    info.pwszName = whost.empty() ? NULL : const_cast<LPWSTR>( whost.c_str() );
    mq[0] = { &IID_IOPCServerList, NULL, S_OK };

    hr = CoCreateInstanceEx( CLSID_OPCEnum, NULL, CLSCTX_REMOTE_SERVER | CLSCTX_LOCAL_SERVER, ( whost.empty() ? NULL : &info ), 1, mq );
    if ( FAILED( hr ) || FAILED( mq[0].hr ) )
    {
      hr = CoCreateInstanceEx( CLSID_OPCEnum, NULL, CLSCTX_INPROC_SERVER, NULL, 1, mq );
      if ( FAILED( hr ) || FAILED( mq[0].hr ) )
      {
        cerr << "Failed to create OPC enum instance. HRESULT: 0x" << hex << hr << endl;
        CoUninitialize();
        return servers;
      }
    }

    server_list = ( IOPCServerList* )mq[0].pItf;
    hr = server_list->EnumClassesOfCategories( 3, da_categories, 0, NULL, &enum_clsid );
    if ( FAILED( hr ) )
    {
      cerr << "Failed to enumerate OPC categories. HRESULT: 0x" << hex << hr << endl;
      CoUninitialize();
      return servers;
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
        info.progid = ( prog_id_ole != nullptr ) ? _bstr_t( prog_id_ole ) : L"";
        info.description = ( user_type_ole != nullptr ) ? _bstr_t( user_type_ole ) : L"";
        info.host = host;

        CoTaskMemFree( prog_id_ole );
        CoTaskMemFree( user_type_ole );
      }
      else
      {
        ostringstream oss;
        oss << "0x" << hex << details_hr;

        info.available = false;
        info.error_code = oss.str();
        info.host = host;
      }

      servers.push_back( info );
    }

    if ( FAILED( hr ) && hr != S_FALSE )
    {
      cerr << "Error during enumeration. HRESULT: 0x" << hex << hr << endl;
    }
  }
  catch ( const exception& e )
  {
    cerr << "Exception during OPC server discovery: " << e.what() << endl;
  }


  if ( com_initialized )
  {
    CoUninitialize();
  }

  return servers;
}

bool OpcDaClient::connect( OPCDA_CONNECT_INFO& info )
{
  string host = info.host.empty() ? "localhost" : info.host;

  if ( !IsEqualCLSID( info.clsid, CLSID_NULL ) )
  {
    if ( connect_clsid( host, info.clsid ) )
    {
      return true;
    }
  }

  if ( !info.progid.empty() )
  {
    if ( connect_progid( host, OPCDA::UTILS::wstr_to_str( info.progid ) ) )
    {
      return true;
    }
  }

  return false;
}

bool OpcDaClient::connect_progid( const string& host_name, const string& progid )
{
  if ( progid.empty() )
  {
    debug( "connect_progid", "ProgID is empty" );
    return false;
  }

  try
  {
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID( OPCDA::UTILS::str_to_wstr( progid ).c_str(), &clsid );

    if ( FAILED( hr ) )
    {
      debug( "connect_progid", hr, "CLSIDFromProgID failed" );
      return false;
    }

    return connect_clsid( host_name, clsid );
  }
  catch ( const exception& e )
  {
    debug( "connect_progid", e );
    return false;
  }
}

bool OpcDaClient::connect_clsid( const string& host_name, const CLSID& server_clsid )
{
  try
  {
    lock_guard<mutex> lock( m_mutex );

    disconnect();

    if ( !is_com_init )
    {
      debug( "connect_clsid", "COM not initialized" );
      return false;
    }

    COSERVERINFO server_info = { 0 };
    wstring w_host_name = host_name.empty() ? L"" : OPCDA::UTILS::str_to_wstr( host_name );
    server_info.pwszName = host_name.empty() ? NULL : const_cast<LPWSTR>( w_host_name.c_str() );

    MULTI_QI mq[1] = {};
    mq[0].pIID = &IID_IOPCServer;
    mq[0].pItf = NULL;
    mq[0].hr = S_OK;


    HRESULT hr = CoCreateInstanceEx( server_clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, ( host_name.empty() ? NULL : &server_info ), 1, mq );


    if ( FAILED( hr ) || FAILED( mq[0].hr ) )
    {
      debug( "CoCreateInstanceEx: server_clsid", hr );

      hr = CoCreateInstanceEx( server_clsid, NULL, CLSCTX_INPROC_SERVER, NULL, 1, mq );
      if ( FAILED( hr ) || FAILED( mq[0].hr ) )
      {
        debug( "connect_clsid second attempt", hr );
        return false;
      }
    }


    m_server = reinterpret_cast<IOPCServer*>( mq[0].pItf );
    if ( !m_server )
    {
      debug( "connect_clsid", "Failed to get valid IOPCServer interface" );
      return false;
    }


    hr = m_server->QueryInterface( IID_IOPCBrowseServerAddressSpace, reinterpret_cast<void**>( &browser ) );
    if ( FAILED( hr ) || !browser )
    {
      debug( "IID_IOPCBrowseServerAddressSpace", hr );


      hr = m_server->QueryInterface( IID_IOPCItemProperties, reinterpret_cast<void**>( &m_item_properties ) );
      if ( FAILED( hr ) || !m_item_properties )
      {
        debug( "IID_IOPCItemProperties", hr );
        debug( "connect_clsid", "Failed to get browsing interfaces" );
        return false;
      }
      else
      {
        m_browse_method = OPCDA_BROWSE_METHOD::ITEM_PROPERTIES;
      }
    }
    else
    {
      m_browse_method = OPCDA_BROWSE_METHOD::SERVER_ADDRESS_SPACE;
    }


    m_default_group = generate_groupname();
    cout << "m_default_group : " << m_default_group << endl;

    if ( !add_opc_group( m_default_group ) )
    {
      debug( "connect_clsid", "Failed to add OPC group" );
      disconnect();
      return false;
    }

    return true;
  }
  catch ( const exception& e )
  {
    debug( "connect_clsid", e );
    disconnect();
    return false;
  }
}

void OpcDaClient::disconnect()
{
  try
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
  catch ( const exception& e )
  {
    debug( "disconnect", e );
  }
}

void OpcDaClient::get_server_status()
{
  try
  {
    lock_guard<mutex> lock( m_mutex );

    auto server_state_to_string = [this]( DWORD state ) -> string
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
          return "UNKNOWN_" + to_string( state );
      }
    };

    if ( m_server )
    {
      if ( !m_status.is_init )
      {
        OPCSERVERSTATUS* ss = nullptr;
        HRESULT hr = m_server->GetStatus( &ss );

        if ( SUCCEEDED( hr ) && ss )
        {
          m_status.is_init = true;
          m_status.server_started_epochtime = OPCDA::UTILS::filetime_to_epochtime( ss->ftStartTime );
          m_status.status_created_epochtime = OPCDA::UTILS::filetime_to_epochtime( ss->ftCurrentTime );
          m_status.status_updated_epochtime = OPCDA::UTILS::filetime_to_epochtime( ss->ftLastUpdateTime );
          m_status.status = ss->dwServerState;
          m_status.status_string = server_state_to_string( ss->dwServerState );
          m_status.enabled_group_len = OPCDA::UTILS::dword_to_int( ss->dwGroupCount );
          m_status.major_version = OPCDA::UTILS::word_to_int( ss->wMajorVersion );
          m_status.minor_version = OPCDA::UTILS::word_to_int( ss->wMinorVersion );
          m_status.build_version = OPCDA::UTILS::word_to_int( ss->wBuildNumber );

          if ( ss->szVendorInfo )
          {
            m_status.vendor = OPCDA::UTILS::wstr_to_str( ss->szVendorInfo );
            CoTaskMemFree( ss->szVendorInfo );
          }
          else
          {
            m_status.vendor = "";
          }

          CoTaskMemFree( ss );
        }
      }
    }
  }
  catch ( const exception& e )
  {
    debug( "get_server_status", e );
  }
}

bool OpcDaClient::is_connected() const
{
  return m_server != nullptr;
}

bool OpcDaClient::add_opc_group( const string& gname )
{
  try
  {
    wstring group_name = OPCDA::UTILS::str_to_wstr( gname );

    if ( !m_server )
    {
      debug( "add_opc_group", "Server interface not available" );
      return false;
    }

    if ( group_name.empty() )
    {
      debug( "add_opc_group", "Group name is empty" );
      return false;
    }

    remove_opc_group();

    DWORD revised_update_rate = 1000;
    OPCHANDLE client_group_handle = 1;

    HRESULT hr = m_server->AddGroup( group_name.c_str(), FALSE, 1000, client_group_handle, NULL, NULL, 0, &m_group_handle_server, &revised_update_rate, IID_IUnknown, &m_group_unknown );

    if ( FAILED( hr ) || !m_group_unknown )
    {
      debug( "AddGroup", hr );
      m_group_unknown = nullptr;
      m_group_handle_server = 0;
      return false;
    }


    hr = m_group_unknown->QueryInterface( IID_IOPCItemMgt, reinterpret_cast<void**>( &m_opc_item_mgt ) );
    if ( FAILED( hr ) || !m_opc_item_mgt )
    {
      debug( "IID_IOPCItemMgt", hr );
      remove_opc_group();
      return false;
    }


    hr = m_group_unknown->QueryInterface( IID_IOPCSyncIO, reinterpret_cast<void**>( &m_opc_sync_io ) );
    if ( FAILED( hr ) || !m_opc_sync_io )
    {
      debug( "IID_IOPCSyncIO", hr );
      remove_opc_group();
      return false;
    }


    hr = m_group_unknown->QueryInterface( IID_IOPCGroupStateMgt, reinterpret_cast<void**>( &m_opc_group_state ) );
    if ( FAILED( hr ) || !m_opc_group_state )
    {
      debug( "IID_IOPCGroupStateMgt", hr );
      remove_opc_group();
      return false;
    }


    BOOL active_state = TRUE;
    DWORD actual_update_rate = 0;
    hr = m_opc_group_state->SetState( &revised_update_rate, &actual_update_rate, &active_state, NULL, NULL, NULL, NULL );

    if ( FAILED( hr ) )
    {
      debug( "SetState", hr );
      remove_opc_group();
      return false;
    }

    return true;
  }
  catch ( const exception& e )
  {
    debug( "add_opc_group", e );
    return false;
  }
}


HRESULT OpcDaClient::browse_tags_iterative( vector<wstring>& all_tags, const wstring& root_path )
{
  if ( !browser || m_browse_method != OPCDA_BROWSE_METHOD::SERVER_ADDRESS_SPACE )
  {
    debug( "browse_tags_iterative", "Browser not available or wrong browse method" );
    return E_FAIL;
  }

  struct BrowsePath
  {
    wstring path;
    int depth;
  };


  vector<BrowsePath> paths_to_browse;
  paths_to_browse.push_back( { root_path, 0 } );

  HRESULT final_result = S_OK;


  set<wstring> processed_paths;


  while ( !paths_to_browse.empty() )
  {

    BrowsePath current = paths_to_browse.back();
    paths_to_browse.pop_back();


    if ( processed_paths.find( current.path ) != processed_paths.end() )
    {
      continue;
    }
    processed_paths.insert( current.path );


    if ( current.depth > m_max_browse_depth )
    {
      debug( "browse_tags_iterative", "Maximum browse depth reached at path: " + OPCDA::UTILS::wstr_to_str( current.path ) );
      continue;
    }


    HRESULT hr = browser->ChangeBrowsePosition( OPC_BROWSE_TO, current.path.empty() ? L"" : current.path.c_str() );

    if ( FAILED( hr ) )
    {
      debug( "ChangeBrowsePosition", hr, "Path: " + OPCDA::UTILS::wstr_to_str( current.path ) );
      continue;
    }


    vector<wstring> leaves;
    CComPtr<IEnumString> leaf_enum;
    hr = browser->BrowseOPCItemIDs( OPC_LEAF, L"", VT_EMPTY, 0, &leaf_enum );

    if ( SUCCEEDED( hr ) && leaf_enum )
    {
      LPOLESTR leaf_name;
      ULONG fetched;

      while ( leaf_enum->Next( 1, &leaf_name, &fetched ) == S_OK && fetched == 1 )
      {
        if ( leaf_name && *leaf_name )
        {

          wstring tag_id = current.path.empty() ? leaf_name : current.path + L"." + leaf_name;


          if ( find( all_tags.begin(), all_tags.end(), tag_id ) == all_tags.end() )
          {
            all_tags.push_back( tag_id );
          }
        }
        CoTaskMemFree( leaf_name );
      }
    }


    CComPtr<IEnumString> branch_enum;
    hr = browser->BrowseOPCItemIDs( OPC_BRANCH, L"", VT_EMPTY, 0, &branch_enum );

    if ( SUCCEEDED( hr ) && branch_enum )
    {
      LPOLESTR branch_name;
      ULONG fetched;

      while ( branch_enum->Next( 1, &branch_name, &fetched ) == S_OK && fetched == 1 )
      {
        if ( branch_name && *branch_name )
        {
          wstring branch_path = current.path.empty() ? branch_name : current.path + L"." + branch_name;


          paths_to_browse.push_back( { branch_path, current.depth + 1 } );
        }
        CoTaskMemFree( branch_name );
      }
    }
  }

  return final_result;
}


HRESULT OpcDaClient::browse_tags( const wstring& path, vector<wstring>& branches, vector<wstring>& tags )
{
  try
  {
    lock_guard<mutex> lock( m_mutex );

    branches.clear();
    tags.clear();

    if ( m_browse_method == OPCDA_BROWSE_METHOD::NONE )
    {
      debug( "browse_tags", "Browse method not initialized" );
      return E_FAIL;
    }
    else if ( m_browse_method == OPCDA_BROWSE_METHOD::SERVER_ADDRESS_SPACE )
    {
      if ( !browser )
      {
        debug( "browse_tags", "IOPCBrowseServerAddressSpace interface not available" );
        return E_NOINTERFACE;
      }


      OPCNAMESPACETYPE namespace_type;
      HRESULT hr = browser->QueryOrganization( &namespace_type );

      if ( FAILED( hr ) )
      {
        debug( "QueryOrganization", hr );
        namespace_type = OPC_NS_HIERARCHIAL;
      }


      if ( namespace_type == OPC_NS_HIERARCHIAL )
      {

        OPCBROWSEDIRECTION direction = path.empty() ? OPC_BROWSE_DOWN : OPC_BROWSE_TO;
        LPWSTR path_ptr = path.empty() ? NULL : const_cast<LPWSTR>( path.c_str() );

        hr = browser->ChangeBrowsePosition( direction, path_ptr );

        if ( FAILED( hr ) )
        {
          debug( "ChangeBrowsePosition", hr, "Failed to change browse position to: " + OPCDA::UTILS::wstr_to_str( path ) );
          return hr;
        }
      }


      CComPtr<IEnumString> branch_enum;
      hr = browser->BrowseOPCItemIDs( OPC_BRANCH, L"", VT_EMPTY, 0, &branch_enum );
      if ( SUCCEEDED( hr ) && branch_enum )
      {
        LPOLESTR branch_name;
        ULONG fetched;
        while ( branch_enum->Next( 1, &branch_name, &fetched ) == S_OK && fetched == 1 )
        {
          if ( branch_name && *branch_name )
          {
            branches.push_back( branch_name );
          }
          CoTaskMemFree( branch_name );
        }
      }


      CComPtr<IEnumString> leaf_enum;
      hr = browser->BrowseOPCItemIDs( OPC_LEAF, L"", VT_EMPTY, 0, &leaf_enum );
      if ( SUCCEEDED( hr ) && leaf_enum )
      {
        LPOLESTR leaf_name;
        ULONG fetched;
        while ( leaf_enum->Next( 1, &leaf_name, &fetched ) == S_OK && fetched == 1 )
        {
          if ( leaf_name && *leaf_name )
          {
            tags.push_back( leaf_name );
          }
          CoTaskMemFree( leaf_name );
        }
      }

      return S_OK;
    }
    else if ( m_browse_method == OPCDA_BROWSE_METHOD::ITEM_PROPERTIES )
    {
      if ( !m_item_properties )
      {
        debug( "browse_tags", "IOPCItemProperties interface not available" );
        return E_NOINTERFACE;
      }


      debug( "browse_tags", "Using IOPCItemProperties instead of IOPCBrowseServerAddressSpace" );
      return E_NOTIMPL;
    }

    return E_FAIL;
  }
  catch ( const exception& e )
  {
    debug( "browse_tags", e );
    return E_FAIL;
  }
}


vector<wstring> OpcDaClient::request_browse_all_tags( vector<wstring>& tags, const wstring& path )
{
  try
  {

    if ( m_browse_method == OPCDA_BROWSE_METHOD::SERVER_ADDRESS_SPACE )
    {
      HRESULT hr = browse_tags_iterative( tags, path );
      if ( FAILED( hr ) )
      {
        debug( "get_all_tags", hr, "Failed to browse tags iteratively" );
      }
    }
    else
    {

      vector<wstring> branches, leaves;
      HRESULT hr = browse_tags( path, branches, leaves );

      if ( SUCCEEDED( hr ) )
      {

        for ( const auto& leaf : leaves )
        {
          try
          {
            wstring id = path.empty() ? leaf : path + L"." + leaf;
            if ( find( tags.begin(), tags.end(), id ) == tags.end() )
            {
              tags.push_back( id );
            }
          }
          catch ( const exception& e )
          {
            debug( "get_all_tags - leaves", e );
          }
        }


        for ( const auto& branch : branches )
        {
          if ( m_browse_depth < m_max_browse_depth )
          {
            try
            {
              m_browse_depth++;
              wstring branch_path = path.empty() ? branch : path + L"." + branch;
              request_browse_all_tags( tags, branch_path );
              m_browse_depth--;
            }
            catch ( const exception& e )
            {
              m_browse_depth--;
              debug( "get_all_tags - branches", e );
            }
          }
          else
          {
            debug( "get_all_tags", "Maximum browse depth reached" );
            break;
          }
        }
      }
    }

    return tags;
  }
  catch ( const exception& e )
  {
    debug( "get_all_tags", e );
    return tags;
  }
}

HRESULT OpcDaClient::resolve_item_id( const wstring& browse_path, wstring& item_id )
{
  try
  {

    auto it = m_id_mapping.find( browse_path );
    if ( it != m_id_mapping.end() )
    {
      item_id = it->second;
      return S_OK;
    }


    if ( browser )
    {
      LPWSTR item_id_str = nullptr;
      HRESULT hr = browser->GetItemID( const_cast<LPWSTR>( browse_path.c_str() ), &item_id_str );

      if ( SUCCEEDED( hr ) && item_id_str )
      {
        wstring candidate = item_id_str;
        CoTaskMemFree( item_id_str );

        if ( validate_item_id( candidate ) )
        {
          item_id = candidate;
          m_id_mapping[browse_path] = item_id;
          return S_OK;
        }
      }
    }


    for ( const auto& pattern : m_id_patterns )
    {
      if ( browse_path.rfind( pattern.first, 0 ) == 0 )
      {
        wstring transformed = browse_path;
        transformed.replace( 0, pattern.first.length(), pattern.second );

        if ( validate_item_id( transformed ) )
        {
          item_id = transformed;
          m_id_mapping[browse_path] = item_id;
          return S_OK;
        }
      }
    }


    vector<wstring> candidates;
    get_item_id_candidates( browse_path, candidates );

    for ( const auto& candidate : candidates )
    {
      if ( validate_item_id( candidate ) )
      {
        item_id = candidate;
        m_id_mapping[browse_path] = item_id;
        learn_id_mapping_pattern( browse_path, candidate );
        return S_OK;
      }
    }


    item_id = browse_path;
    return S_FALSE;
  }
  catch ( const exception& e )
  {
    debug( "resolve_item_id", e );
    item_id = browse_path;
    return E_FAIL;
  }
}


bool OpcDaClient::validate_item_id( const wstring& item_id )
{
  try
  {
    if ( !m_opc_item_mgt )
    {
      return false;
    }

    OPCITEMDEF item_def = { 0 };
    item_def.szAccessPath = L"";
    item_def.szItemID = const_cast<LPWSTR>( item_id.c_str() );
    item_def.bActive = FALSE;
    item_def.hClient = 1;
    item_def.dwBlobSize = 0;
    item_def.pBlob = NULL;
    item_def.vtRequestedDataType = VT_EMPTY;

    OPCITEMRESULT* result = nullptr;
    HRESULT* errors = nullptr;

    HRESULT hr = m_opc_item_mgt->ValidateItems( 1, &item_def, FALSE, &result, &errors );

    bool valid = SUCCEEDED( hr ) && errors && SUCCEEDED( errors[0] );


    if ( result )
    {
      if ( result->pBlob )
      {
        CoTaskMemFree( result->pBlob );
      }
      CoTaskMemFree( result );
    }
    if ( errors )
    {
      CoTaskMemFree( errors );
    }

    return valid;
  }
  catch ( const exception& e )
  {
    debug( "validate_item_id", e );
    return false;
  }
}


void OpcDaClient::get_item_id_candidates( const wstring& browse_path, vector<wstring>& candidates )
{
  try
  {

    candidates.push_back( browse_path );


    vector<wstring> components;
    size_t start = 0, end = 0;

    while ( ( end = browse_path.find( L'.', start ) ) != wstring::npos )
    {
      components.push_back( browse_path.substr( start, end - start ) );
      start = end + 1;
    }
    components.push_back( browse_path.substr( start ) );


    if ( components.size() > 1 )
    {
      wstring without_first;
      for ( size_t i = 1; i < components.size(); i++ )
      {
        if ( i > 1 )
        {
          without_first += L".";
        }
        without_first += components[i];
      }
      candidates.push_back( without_first );
    }


    for ( size_t i = 0; i < components.size(); i++ )
    {
      wstring partial;
      for ( size_t j = i; j < components.size(); j++ )
      {
        if ( j > i )
        {
          partial += L".";
        }
        partial += components[j];
      }
      if ( !partial.empty() && find( candidates.begin(), candidates.end(), partial ) == candidates.end() )
      {
        candidates.push_back( partial );
      }
    }


    if ( !components.empty() )
    {
      wstring last_component = components.back();
      if ( find( candidates.begin(), candidates.end(), last_component ) == candidates.end() )
      {
        candidates.push_back( last_component );
      }
    }
  }
  catch ( const exception& e )
  {
    debug( "get_item_id_candidates", e );
  }
}


void OpcDaClient::learn_id_mapping_pattern( const wstring& browse_path, const wstring& valid_id )
{
  try
  {

    size_t orig_first_dot = browse_path.find( L'.' );
    size_t valid_first_dot = valid_id.find( L'.' );

    if ( orig_first_dot != wstring::npos && ( valid_first_dot != wstring::npos || valid_id.find( L'.' ) == wstring::npos ) )
    {

      if ( browse_path.substr( orig_first_dot ) == valid_id || browse_path.substr( orig_first_dot + 1 ) == valid_id )
      {
        wstring prefix = browse_path.substr( 0, orig_first_dot + 1 );
        wstring replacement = L"";


        bool pattern_exists = false;
        for ( const auto& pattern : m_id_patterns )
        {
          if ( pattern.first == prefix )
          {
            pattern_exists = true;
            break;
          }
        }

        if ( !pattern_exists )
        {
          debug( "learn_id_mapping_pattern", "Found pattern: '" + OPCDA::UTILS::wstr_to_str( prefix ) + "' -> '" + OPCDA::UTILS::wstr_to_str( replacement ) + "'" );
          m_id_patterns.push_back( make_pair( prefix, replacement ) );
        }
      }
    }
  }
  catch ( const exception& e )
  {
    debug( "learn_id_mapping_pattern", e );
  }
}


void OpcDaClient::request_readable_tags( const wstring& path )
{
  try
  {
    m_available_tags.clear();
    m_all_tags.clear();
    m_browse_depth = 0;


    vector<wstring> tag_list;
    request_browse_all_tags( tag_list, path );

    debug( "get_readable_tags", "Found " + to_string( tag_list.size() ) + " total tags" );


    for ( const auto& browse_path : tag_list )
    {
      wstring item_id;
      HRESULT hr = resolve_item_id( browse_path, item_id );


      m_all_tags.push_back( browse_path );

      if ( SUCCEEDED( hr ) )
      {
        wstring resolved_tag = item_id;
        m_available_tags.push_back( resolved_tag );
      }
    }

    debug( "get_readable_tags", "Resolved " + to_string( m_available_tags.size() ) + " readable tags from " + to_string( m_all_tags.size() ) + " total tags" );
  }
  catch ( const exception& e )
  {
    debug( "get_readable_tags", e );
  }
}

bool OpcDaClient::validate_and_add_tag( const wstring& item_id )
{
  try
  {
    if ( !m_opc_item_mgt )
    {
      return false;
    }

    OPCITEMDEF item_def = { 0 };
    item_def.szAccessPath = L"";
    item_def.szItemID = const_cast<LPWSTR>( item_id.c_str() );
    item_def.bActive = TRUE;
    item_def.hClient = 1;
    item_def.dwBlobSize = 0;
    item_def.pBlob = NULL;
    item_def.vtRequestedDataType = VT_EMPTY;

    OPCITEMRESULT* item_result = nullptr;
    HRESULT* item_errors = nullptr;

    HRESULT hr = m_opc_item_mgt->ValidateItems( 1, &item_def, FALSE, &item_result, &item_errors );

    bool valid = SUCCEEDED( hr ) && item_errors && SUCCEEDED( item_errors[0] );


    if ( item_result )
    {
      if ( item_result->pBlob )
      {
        CoTaskMemFree( item_result->pBlob );
      }
      CoTaskMemFree( item_result );
    }

    if ( item_errors )
    {
      CoTaskMemFree( item_errors );
    }

    return valid;
  }
  catch ( const exception& e )
  {
    debug( "validate_and_add_tag", e );
    return false;
  }
}

HRESULT OpcDaClient::read_sync( const vector<wstring>& item_ids, vector<OPCDA_TAG>& results, vector<HRESULT>& errors )
{
  try
  {
    results.clear();
    errors.clear();

    if ( !m_opc_sync_io || !m_opc_item_mgt )
    {
      debug( "read_sync", "!m_opc_sync_io || !m_opc_item_mgt" );
      return E_POINTER;
    }

    if ( item_ids.empty() )
    {
      return S_OK;
    }

    lock_guard<mutex> lock( m_mutex );

    DWORD count = static_cast<DWORD>( item_ids.size() );
    vector<OPCITEMDEF> item_defs( count );


    vector<wstring> resolved_ids( count );

    for ( DWORD i = 0; i < count; ++i )
    {
      wstring browse_path = item_ids[i];
      wstring item_id;


      auto it = m_id_mapping.find( browse_path );
      if ( it != m_id_mapping.end() )
      {
        item_id = it->second;
      }
      else
      {
        resolve_item_id( browse_path, item_id );
      }

      resolved_ids[i] = item_id;

      ZeroMemory( &item_defs[i], sizeof( OPCITEMDEF ) );

      item_defs[i].szAccessPath = L"";
      item_defs[i].szItemID = const_cast<LPWSTR>( item_id.c_str() );
      item_defs[i].bActive = TRUE;
      item_defs[i].hClient = i + 1;
      item_defs[i].dwBlobSize = 0;
      item_defs[i].pBlob = NULL;
      item_defs[i].vtRequestedDataType = VT_EMPTY;
    }

    results.resize( count );
    errors.resize( count );

    OPCITEMRESULT* add_results = nullptr;
    HRESULT* pAddErrors = nullptr;
    HRESULT hr;

    {
      hr = m_opc_item_mgt->AddItems( count, item_defs.data(), &add_results, &pAddErrors );
      if ( FAILED( hr ) )
      {
        debug( "AddItems", hr );

        if ( add_results )
        {
          for ( DWORD i = 0; i < count; ++i )
          {
            if ( add_results[i].pBlob )
            {
              CoTaskMemFree( add_results[i].pBlob );
            }
          }

          CoTaskMemFree( add_results );
        }

        if ( pAddErrors )
        {
          CoTaskMemFree( pAddErrors );
        }

        fill( errors.begin(), errors.end(), hr );
        return hr;
      }
    }


    vector<OPCHANDLE> server_handles( count );
    {
      for ( DWORD i = 0; i < count; ++i )
      {
        errors[i] = pAddErrors[i];
        results[i].id = item_ids[i];

        if ( SUCCEEDED( pAddErrors[i] ) && add_results )
        {
          server_handles[i] = add_results[i].hServer;
          results[i].access_rights = add_results[i].dwAccessRights;
          results[i].data_type = add_results[i].vtCanonicalDataType;
        }
        else
        {
          server_handles[i] = 0;

          if ( SUCCEEDED( errors[i] ) )
          {
            errors[i] = E_FAIL;
          }

          // wcerr << L"Error: Failed to add item '" << item_ids[i] << L"'. HRESULT: 0x" << hex << errors[i] << endl;

          results[i].access_rights = 0;
          results[i].data_type = VT_EMPTY;
          results[i].quality = OPC_QUALITY_BAD;

          VariantInit( &results[i].value );
          memset( &results[i].timestamp, 0, sizeof( FILETIME ) );
        }
      }
    }


    if ( add_results )
    {
      for ( DWORD i = 0; i < count; ++i )
      {
        if ( add_results[i].pBlob )
        {
          CoTaskMemFree( add_results[i].pBlob );
        }
      }
      CoTaskMemFree( add_results );
    }

    if ( pAddErrors )
    {
      CoTaskMemFree( pAddErrors );
    }


    vector<OPCHANDLE> valid_server_handles;
    vector<DWORD> original_indices;
    for ( DWORD i = 0; i < count; ++i )
    {
      if ( server_handles[i] != 0 )
      {
        valid_server_handles.push_back( server_handles[i] );
        original_indices.push_back( i );
      }
    }


    if ( !valid_server_handles.empty() )
    {
      OPCITEMSTATE* item_states = nullptr;
      HRESULT* pReadErrors = nullptr;
      DWORD valid_count = static_cast<DWORD>( valid_server_handles.size() );

      hr = m_opc_sync_io->Read( OPC_DS_CACHE, valid_count, valid_server_handles.data(), &item_states, &pReadErrors );
      if ( SUCCEEDED( hr ) )
      {
        for ( DWORD i = 0; i < valid_count; ++i )
        {
          DWORD original_idx = original_indices[i];
          errors[original_idx] = pReadErrors[i];

          if ( SUCCEEDED( pReadErrors[i] ) )
          {
            results[original_idx].value = item_states[i].vDataValue;
            results[original_idx].quality = item_states[i].wQuality;
            results[original_idx].timestamp = item_states[i].ftTimeStamp;
            results[original_idx].data_type = item_states[i].vDataValue.vt;
          }
          else
          {
            wcerr << L"Error: Failed to read item '" << item_ids[original_idx] << L"'. HRESULT: 0x" << hex << pReadErrors[i] << endl;
            results[original_idx].quality = OPC_QUALITY_BAD;
            VariantClear( &results[original_idx].value );
            memset( &results[original_idx].timestamp, 0, sizeof( FILETIME ) );
          }

          VariantClear( &item_states[i].vDataValue );
        }
      }
      else
      {
        debug( "IOPCSyncIO::Read", hr );
        for ( DWORD idx : original_indices )
        {
          errors[idx] = hr;
          results[idx].quality = OPC_QUALITY_BAD;
          VariantClear( &results[idx].value );
          memset( &results[idx].timestamp, 0, sizeof( FILETIME ) );
        }
      }

      if ( item_states )
      {
        CoTaskMemFree( item_states );
      }

      if ( pReadErrors )
      {
        CoTaskMemFree( pReadErrors );
      }
    }


    if ( !valid_server_handles.empty() )
    {
      HRESULT* hr_remove_errs = nullptr;
      HRESULT hr_remove = m_opc_item_mgt->RemoveItems( static_cast<DWORD>( valid_server_handles.size() ), valid_server_handles.data(), &hr_remove_errs );
      if ( FAILED( hr_remove ) )
      {
        debug( "RemoveItems", hr_remove );
      }

      if ( hr_remove_errs )
      {
        CoTaskMemFree( hr_remove_errs );
      }
    }

    bool any_failed = false;
    for ( HRESULT item_hr : errors )
    {
      if ( FAILED( item_hr ) )
      {
        any_failed = true;
        break;
      }
    }
    return any_failed ? S_FALSE : S_OK;
  }
  catch ( const exception& e )
  {
    debug( "read_sync", e );
    return E_FAIL;
  }
}

void OpcDaClient::remove_opc_group()
{
  try
  {
    if ( m_opc_sync_io )
    {
      m_opc_sync_io.Release();
    }

    if ( m_opc_item_mgt )
    {
      m_opc_item_mgt.Release();
    }

    if ( m_opc_group_state )
    {
      m_opc_group_state.Release();
    }

    if ( m_group_unknown )
    {
      m_group_unknown.Release();
    }

    if ( m_server && m_group_handle_server != 0 )
    {
      HRESULT hr = m_server->RemoveGroup( m_group_handle_server, FALSE );
      if ( FAILED( hr ) )
      {
        debug( "RemoveGroup", hr );
      }

      m_group_handle_server = 0;
    }
  }
  catch ( const exception& e )
  {
    debug( "remove_opc_group", e );
  }
}

HRESULT OpcDaClient::get_item_properties( const wstring& item_id, OPCDA_TAG& properties )
{
  try
  {
    if ( !m_server )
    {
      return E_POINTER;
    }

    CComPtr<IOPCItemProperties> item_props;
    HRESULT hr = m_server->QueryInterface( IID_IOPCItemProperties, ( void** )&item_props );
    if ( FAILED( hr ) )
    {
      wcerr << L"Warning: Server does not support IOPCItemProperties. HRESULT: 0x" << hex << hr << endl;
      return hr;
    }

    const DWORD OPC_PROP_DATATYPE = 1;
    const DWORD OPC_PROP_RIGHTS = 5;
    DWORD property_ids_to_query[] = { OPC_PROP_DATATYPE, OPC_PROP_RIGHTS };
    DWORD num_properties = sizeof( property_ids_to_query ) / sizeof( DWORD );

    LPWSTR item_id_cstr = const_cast<LPWSTR>( item_id.c_str() );
    VARIANT* property_values = nullptr;
    HRESULT* pPropertyErrors = nullptr;

    hr = item_props->GetItemProperties( item_id_cstr, num_properties, property_ids_to_query, &property_values, &pPropertyErrors );

    properties.id = item_id;
    properties.data_type = VT_EMPTY;
    properties.access_rights = 0;
    VariantInit( &properties.value );
    properties.quality = OPC_QUALITY_BAD;
    memset( &properties.timestamp, 0, sizeof( FILETIME ) );

    if ( FAILED( hr ) )
    {
      wcerr << L"Error: GetItemProperties call failed for item '" << item_id.c_str() << L"'. HRESULT: 0x" << hex << hr << endl;
      if ( property_values )
      {
        CoTaskMemFree( property_values );
      }
      if ( pPropertyErrors )
      {
        CoTaskMemFree( pPropertyErrors );
      }
      return hr;
    }

    vector<HRESULT> property_errors( num_properties );
    if ( pPropertyErrors )
    {
      memcpy( property_errors.data(), pPropertyErrors, num_properties * sizeof( HRESULT ) );
    }
    else
    {
      fill( property_errors.begin(), property_errors.end(), E_UNEXPECTED );
    }

    for ( DWORD i = 0; i < num_properties; ++i )
    {
      if ( SUCCEEDED( property_errors[i] ) && property_values )
      {
        switch ( property_ids_to_query[i] )
        {
          case OPC_PROP_DATATYPE:
            if ( property_values[i].vt == VT_I2 || property_values[i].vt == VT_UI2 )
            {
              properties.data_type = static_cast<VARTYPE>( property_values[i].iVal );
            }
            else
            {
              wcerr << L"Warning: Unexpected VARIANT type (" << property_values[i].vt << L") for OPC_PROP_DATATYPE on item " << item_id.c_str() << endl;
            }
            break;
          case OPC_PROP_RIGHTS:
            if ( property_values[i].vt == VT_I4 || property_values[i].vt == VT_UI4 )
            {
              properties.access_rights = property_values[i].ulVal;
            }
            else
            {
              wcerr << L"Warning: Unexpected VARIANT type (" << property_values[i].vt << L") for OPC_PROP_RIGHTS on item " << item_id.c_str() << endl;
            }
            break;
        }
        VariantClear( &property_values[i] );
      }
      else
      {
        wcerr << L"Error: Failed to get property ID " << property_ids_to_query[i] << L" for item '" << item_id.c_str() << L"'. HRESULT: 0x" << hex << property_errors[i] << endl;
      }
    }

    if ( property_values )
    {
      CoTaskMemFree( property_values );
    }
    if ( pPropertyErrors )
    {
      CoTaskMemFree( pPropertyErrors );
    }

    bool any_prop_failed = false;
    for ( HRESULT prop_hr : property_errors )
    {
      if ( FAILED( prop_hr ) )
      {
        any_prop_failed = true;
        break;
      }
    }
    return any_prop_failed ? S_FALSE : S_OK;
  }
  catch ( const exception& e )
  {
    debug( "get_item_properties", e );
    return E_FAIL;
  }
}