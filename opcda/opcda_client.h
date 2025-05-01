// opcda_client.h
#ifndef OPCDA_CLIENT_H
#define OPCDA_CLIENT_H

#include <Shlwapi.h>
#include <atlbase.h>
#include <comdef.h>
#include <map>
#include <memory>
#include <mutex>
#include <opcda.h>
#include <set>
#include <string>
#include <variant>
#include <vector>

using namespace std;

#ifndef CLSID_OPCServerList
DEFINE_GUID( CLSID_OPCServerList, 0x13486D51, 0x4821, 0x11D2, 0xA4, 0x94, 0x3C, 0xB3, 0x06, 0xC1, 0x00, 0x00 );
#endif

#ifndef IID_IOPCServer
DEFINE_GUID( IID_IOPCServer, 0x39c13a4d, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3 );
#endif

#ifndef IID_IOPCServerList
DEFINE_GUID( IID_IOPCServerList, 0x13486D51, 0x4822, 0x11D2, 0xA4, 0x94, 0x3C, 0xB3, 0x06, 0xC1, 0x00, 0x00 );
#endif

#ifndef IID_IOPCBrowseServerAddressSpace
DEFINE_GUID( IID_IOPCBrowseServerAddressSpace, 0x39C13A4F, 0x011E, 0x11D0, 0x96, 0x75, 0x00, 0x20, 0xAF, 0xD8, 0xAD, 0xB3 );
#endif

#ifndef IID_IOPCSyncIO
DEFINE_GUID( IID_IOPCSyncIO, 0x39C13A52, 0x011E, 0x11D0, 0x96, 0x75, 0x00, 0x20, 0xAF, 0xD8, 0xAD, 0xB3 );
#endif

#ifndef IID_IOPCItemMgt
DEFINE_GUID( IID_IOPCItemMgt, 0x39C13A4D, 0x011E, 0x11D0, 0x96, 0x75, 0x00, 0x20, 0xAF, 0xD8, 0xAD, 0xB3 );
#endif

#ifndef IID_IOPCItemProperties
DEFINE_GUID( IID_IOPCItemProperties, 0x39c13a72, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3 );
#endif

#ifndef IID_IOPCBrowse
DEFINE_GUID( IID_IOPCBrowse, 0x39227004, 0xA18F, 0x4B57, 0xA8, 0x5A, 0xF8, 0x24, 0x13, 0x43, 0x7B, 0x33 );
#endif

#ifndef CLSID_OPCEnum

static const CLSID CLSID_OPCEnum = { 0x13486D51, 0x4821, 0x11D2, { 0xA4, 0x94, 0x3C, 0xB3, 0x06, 0xC1, 0x00, 0x00 } };
#endif

#ifndef CATID_OPCDAServer10

const CATID CATID_OPCDAServer10 = { 0x63D5F430, 0xCFE4, 0x11d1, { 0xB2, 0xC8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
#endif

#ifndef CATID_OPCDAServer20

const CATID CATID_OPCDAServer20 = { 0x63D5F432, 0xCFE4, 0x11d1, { 0xB2, 0xC8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
#endif

#ifndef CATID_OPCDAServer30

const CATID CATID_OPCDAServer30 = { 0xCC603642, 0x66D7, 0x48f1, { 0xB6, 0x9A, 0xB6, 0x25, 0xE7, 0x36, 0x52, 0xD7 } };
#endif

constexpr GUID CATID_DA10 = { 0xF31DFDE1, 0x07B6, 0x11D2, { 0xB2, 0xD8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
constexpr GUID CATID_DA20 = { 0x63D5F430, 0xCFE4, 0x11D1, { 0xB2, 0xC8, 0x00, 0x60, 0x08, 0x3B, 0xA1, 0xFB } };
constexpr GUID CATID_DA30 = { 0xB28A2600, 0x4A0F, 0x11D2, { 0xB1, 0x89, 0x00, 0xA0, 0xC9, 0x69, 0xE1, 0x2E } };

#pragma comment( lib, "Shlwapi.lib" )

constexpr int DEFAULT_MAX_BROWSE_DEPTH = 32;
constexpr size_t DEFAULT_MAX_STRING_BUFFER = 4096;

using namespace std;

enum class OPCDA_BROWSE_METHOD
{
  NONE,
  SERVER_ADDRESS_SPACE,
  ITEM_PROPERTIES
};

struct OPCDA_CONNECT_INFO
{
  bool available = false;
  string host;
  wstring progid;
  CLSID clsid = CLSID_NULL;
  wstring description;
  string error_code;
};

struct OPCDA_TAG
{
  wstring id;
  VARIANT value;
  WORD quality = 0;
  FILETIME timestamp = { 0, 0 };
  VARTYPE data_type = VT_EMPTY;
  DWORD access_rights = 0;
};

struct ServerStatus
{
  bool is_init = false;
  string vendor;
  int major_version = 0;
  int minor_version = 0;
  int build_version = 0;
  long long server_started_epochtime = 0;
  long long status_created_epochtime = 0;
  long long status_updated_epochtime = 0;
  DWORD status = 0;
  string status_string;
  int enabled_group_len = 0;
};

class OpcDaClient
{
public:
  OpcDaClient();
  ~OpcDaClient();


  void set_max_browse_depth( int depth );
  int get_max_browse_depth() const
  {
    return m_max_browse_depth;
  }
  size_t get_max_string_buffer() const
  {
    return m_max_string_buffer;
  }


  ServerStatus m_status;
  vector<wstring> m_available_tags;
  vector<wstring> m_all_tags;


  bool com_init();
  void com_free();


  static vector<OPCDA_CONNECT_INFO> discovery( const string& host = "localhost" );
  bool connect( OPCDA_CONNECT_INFO& info );
  bool connect_progid( const string& host_name, const string& progid );
  bool connect_clsid( const string& host_name, const CLSID& server_clsid );
  void disconnect();
  bool is_connected() const;


  void get_server_status();


  bool add_opc_group( const string& gname );
  void remove_opc_group();


  HRESULT browse_tags( const wstring& path, vector<wstring>& branches, vector<wstring>& tags );
  vector<wstring> request_browse_all_tags( vector<wstring>& tags, const wstring& path = L"" );
  void request_readable_tags( const wstring& path = L"" );
  bool validate_and_add_tag( const wstring& item_id );


  HRESULT resolve_item_id( const wstring& browse_path, wstring& item_id );
  bool validate_item_id( const wstring& item_id );
  void get_item_id_candidates( const wstring& browse_path, vector<wstring>& candidates );
  void learn_id_mapping_pattern( const wstring& browse_path, const wstring& valid_id );


  HRESULT read_sync( const vector<wstring>& item_ids, vector<OPCDA_TAG>& results, vector<HRESULT>& errors );
  HRESULT get_item_properties( const wstring& item_id, OPCDA_TAG& properties );


  void debug( const string& tag, HRESULT& hr, const string& message = "" );
  void debug( const string& tag, const string& message );
  void debug( const string& tag, const exception& message );
  void debug( const string& message );

private:
  bool is_com_init;
  mutable mutex m_mutex;
  int m_browse_depth = 0;
  int m_max_browse_depth = DEFAULT_MAX_BROWSE_DEPTH;
  size_t m_max_string_buffer = DEFAULT_MAX_STRING_BUFFER;

  CComPtr<IOPCServer> m_server;
  CComPtr<IOPCBrowseServerAddressSpace> browser;
  CComPtr<IOPCItemProperties> m_item_properties;
  CComPtr<IUnknown> m_group_unknown;
  CComPtr<IOPCItemMgt> m_opc_item_mgt;
  CComPtr<IOPCSyncIO> m_opc_sync_io;
  CComPtr<IOPCGroupStateMgt> m_opc_group_state;

  OPCHANDLE m_group_handle_server;
  string m_default_group;


  OPCDA_BROWSE_METHOD m_browse_method = OPCDA_BROWSE_METHOD::NONE;
  map<wstring, wstring> m_id_mapping;
  vector<pair<wstring, wstring>> m_id_patterns;


  HRESULT browse_tags_iterative( vector<wstring>& all_tags, const wstring& root_path = L"" );
};
#endif