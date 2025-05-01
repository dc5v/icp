#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#include "opcda_cli.h"
#include "crash_handler.h"
#include "opcda_utils.h"
#include "result_formatter.hpp"

using namespace std;

namespace OPCDA::CLI
{
  static int discovery( const string& host = "localhost" )
  {
    auto servers = OpcDaClient::discovery( host );

    map<string, map<string, string>> server_map;

    for ( const auto& s : servers )
    {
      wstring progid = s.progid;

      wchar_t* clsid_str = nullptr;
      StringFromCLSID( s.clsid, &clsid_str );

      server_map[OPCDA::UTILS::wstr_to_str( progid )] = { { "clsid", OPCDA::UTILS::wstr_to_str( clsid_str ? clsid_str : L"" ) } };

      if ( clsid_str )
      {
        CoTaskMemFree( clsid_str );
      }
    }

    ResultFormatter::getInstance().printOpcServers( server_map );
    return 0;
  }

  static int browse_tags( OpcDaClient& client, bool with_status = false, bool only_readable = false )
  {
    if ( with_status )
    {
      client.get_server_status();
    }

    vector<wstring> tags;

    if ( only_readable )
    {
      client.request_readable_tags( L"" );
    }
    else
    {
      client.request_browse_all_tags( tags, L"" );
    }

    if ( tags.empty() )
    {
      return 1;
    }

    vector<string> tagList;
    for ( const auto& t : tags )
    {
      tagList.push_back( OPCDA::UTILS::wstr_to_str( t ) );
    }

    ResultFormatter::getInstance().printTags( tagList );
    return 0;
  }

  static int read_tag_values( OpcDaClient& client, const vector<wstring>& tags, const vector<wstring>& columns, bool showStatus )
  {
    if ( tags.empty() )
    {
      return 1;
    }

    vector<OPCDA_TAG> results;
    vector<HRESULT> errors;

    if ( FAILED( client.read_sync( tags, results, errors ) ) )
    {
      return 1;
    }

    map<string, OPCDA_TAG> result;

    for ( const auto& tag : results )
    {
      result[OPCDA::UTILS::wstr_to_str( tag.id )] = tag;
    }

    ResultFormatter::getInstance().printTagValues( result );
    return 0;
  }

  static int subscribe_on_change( OpcDaClient& client, const vector<wstring>& excludes, const vector<wstring>& columns, int intervalMs, bool showStatus )
  {
    return 0;
  }

  static int dialog_session( OpcDaClient& client, const vector<wstring>& columns, bool showStatus )
  {
    return 0;
  }

  OPCDA::CLI::Commands command_to_enum( const string& cmd )
  {
    if ( cmd == "--discovery" )
    {
      return OPCDA::CLI::Commands::Discovery;
    }
    else if ( cmd == "--browse-tags" )
    {
      return OPCDA::CLI::Commands::BrowseAll;
    }
    else if ( cmd == "--browse-tags-readable" )
    {
      return OPCDA::CLI::Commands::BrowseReadable;
    }
    else if ( cmd == "--tag-values" )
    {
      return OPCDA::CLI::Commands::TagValues;
    }
    else if ( cmd == "--subscribe" )
    {
      return OPCDA::CLI::Commands::Subscribe;
    }
    else if ( cmd == "--dialog" )
    {
      return OPCDA::CLI::Commands::Dialog;
    }

    return OPCDA::CLI::Commands::NotSet;
  }

  bool parse_arguments( int argc, char* argv[], OPCDA::CLI::OptionParams& o )
  {
    if ( argc < 2 )
    {
      return false;
    }

    // @brief CLI - log settings
    {
      for ( int i = 1; i < argc; ++i )
      {
        string arg = argv[i];

        if ( arg == "--help" )
        {
          return false;
        }

        if ( arg == "--logs" )
        {
          o.log_mode = LogMode::CONSOLE;
        }

        else if ( arg == "--logs-buffer" )
        {
          o.log_mode = LogMode::BUFFER;
        }

        else if ( arg == "--logs-file" && i + 1 < argc && argv[i + 1][0] != '-' )
        {
          o.log_mode = LogMode::FILE;
          o.log_file = argv[++i];
        }
      }

      Logger::instance().set_mode( o.log_mode, o.log_file );
    }

    for ( int i = 1; i < argc; ++i )
    {
      OPCDA::CLI::Commands c = command_to_enum( argv[i] );

      if ( c != OPCDA::CLI::Commands::NotSet )
      {
        o.cmd = c;
      }
    }

    if ( o.cmd == OPCDA::CLI::Commands::NotSet )
    {
      o.cmd = OPCDA::CLI::Commands::Help;
      return false;
    }

    auto getVal = [&]( const string& f, const string& d = "" ) -> string
    {
      for ( int i = 1; i < argc - 1; ++i )
        if ( f == argv[i] && argv[i + 1][0] != '-' )
          return argv[i + 1];
      return d;
    };
    o.conn.host = getVal( "--host", "localhost" );
    o.conn.progid = getVal( "--progid" );
    o.conn.clsid = getVal( "--clsid" );
    o.interval_ms = stoi( getVal( "--interval", "1000" ) );
    o.show_status = any_of( argv + 1, argv + argc, []( char* a ) { return string( a ) == "--status"; } );

    return true;
  }

  int commander( const OPCDA::CLI::OptionParams& o )
  {
    setupCrashHandler();

    OpcDaClient client;

    if ( !client.com_init() )
    {
      cerr << "COM init failed";
      return 1;
    }

    switch ( o.cmd )
    {
      case OPCDA::CLI::Commands::Discovery:
        return discovery( o.conn.host );

      case OPCDA::CLI::Commands::BrowseAll:
        return browse_tags( client, o.show_status, false );

      case OPCDA::CLI::Commands::BrowseReadable:
        return browse_tags( client, o.show_status, true );

      case OPCDA::CLI::Commands::TagValues:
        return read_tag_values( client, {}, o.columns, o.show_status );

      case OPCDA::CLI::Commands::Subscribe:
        return subscribe_on_change( client, o.excludes, o.columns, o.interval_ms, o.show_status );

      case OPCDA::CLI::Commands::Dialog:
        return dialog_session( client, o.columns, o.show_status );

      default:
        help();
        return 0;
    }
  }

  void help()
  {
    cout << "Usage: opcda-cli.exe <COMMAND> [OPTIONS]\n\n"
         << "COMMANDS:\n"
         << "  --discovery            Discover servers\n"
         << "  --browse-tags          List all tags\n"
         << "  --browse-tags-readable List readable tags\n"
         << "  --tag-values           Read tag values\n"
         << "  --subscribe            Subscribe to tag changes\n"
         << "  --dialog               Interactive mode\n";
  }
} // namespace OPCDA::CLI
