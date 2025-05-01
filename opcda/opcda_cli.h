// opcda_cli.h
#ifndef OPCDA_CLI_H
#define OPCDA_CLI_H

#include <string>
#include <vector>

#include "logger.h"
#include "opcda_client.h"

using namespace std;

namespace OPCDA::CLI
{
  struct ConnectionParams
  {
    string host = "localhost";
    string progid;
    string clsid;
  };

  enum class Commands
  {
    Help,
    Discovery,
    BrowseAll,
    BrowseReadable,
    TagValues,
    Subscribe,
    Dialog,
    NotSet
  };

  struct OptionParams
  {
    Commands cmd = Commands::Help;
    ConnectionParams conn;
    vector<string> tags;
    vector<wstring> excludes;
    vector<wstring> columns;

    int interval_ms = 1000;
    bool show_status = false;
    LogMode log_mode = LogMode::NONE;
    string log_file = "opcda_client.log";
  };

  bool parseArguments( int argc, char* argv[], OptionParams& opts );
  int commander( const OptionParams& opts );
  void help();

} // namespace OPCDA::CLI

#endif // OPCDA_CLI_H
