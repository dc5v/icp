// result_formatter.hpp
#ifndef RESULT_FORMATTER_HPP
#define RESULT_FORMATTER_HPP

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "opcda_client.h"
#include "opcda_utils.h"

using namespace std;

class ResultFormatter
{
public:
  static ResultFormatter& getInstance();

  template <typename T>
  void printSuccess( const map<string, T>& results )
  {
    cout << "success: true" << endl;
    cout << "result:" << endl;
    for ( const auto& pair : results )
    {
      cout << "  " << pair.first << ": " << pair.second << endl;
    }
  }

  template <typename T>
  void printSuccessIndexed( const vector<T>& results )
  {
    cout << "success: true" << endl;
    cout << "result:" << endl;
    for ( size_t i = 0; i < results.size(); ++i )
    {
      cout << "  " << ( i + 1 ) << ": " << results[i] << endl;
    }
  }


  void printError( int code, const string& message )
  {
    cout << "success: false" << endl;
    cout << "error:" << endl;
    cout << "  code: " << code << endl;
    cout << "  message: " << message << endl;
  }


  void printLogs( const vector<string>& logs )
  {
    if ( !logs.empty() )
    {
      cout << "logs:" << endl;
      for ( const auto& log : logs )
      {
        cout << "  - " << log << endl;
      }
    }
  }


  void printOpcServers( const map<string, map<string, string>>& servers )
  {
    cout << "success: true" << endl;
    cout << "result:" << endl;
    int index = 1;
    for ( const auto& server : servers )
    {
      cout << "  " << index++ << ":" << endl;
      for ( const auto& attr : server.second )
      {
        cout << "    " << attr.first << ": " << attr.second << endl;
      }
    }
  }

  void printTags( const vector<string>& tags )
  {
    cout << "success: true" << endl;
    cout << "result:" << endl;
    for ( size_t i = 0; i < tags.size(); ++i )
    {
      cout << "  " << ( i + 1 ) << ": " << tags[i] << endl;
    }
  }

  void printTagValues( const map<string, OPCDA_TAG>& values )
  {
    cout << "success: true" << endl;
    cout << "result:" << endl;

    for ( const auto& tag : values )
    {
      cout << "  " << tag.first << ":" << endl;

      VARIANT tag_value = tag.second.value;
      VARTYPE tag_type = tag.second.data_type;
      string tab = "    ";

      // tag.second.id
      // tag.second.timestamp
      cout << tab << "- id: " << OPCDA::UTILS::wstr_to_str( tag.second.id ) << endl;
      cout << tab << "- value: " << OPCDA::UTILS::variant_to_str( tag_value ) << endl;
      cout << tab << "- data_type: " << OPCDA::UTILS::vartype_to_str( tag_type ) << endl;
      cout << tab << "- timestamp: " << OPCDA::UTILS::filetime_to_epochtime( tag.second.timestamp ) << endl;
      cout << tab << "- isotime: " << OPCDA::UTILS::filetime_to_isotime( tag.second.timestamp ) << endl;

      VariantClear( &tag_value );
    }
  }

private:
  ResultFormatter()
  {
  }
  ~ResultFormatter()
  {
  }

  ResultFormatter( const ResultFormatter& ) = delete;
  ResultFormatter& operator=( const ResultFormatter& ) = delete;
  ResultFormatter( ResultFormatter&& ) = delete;
  ResultFormatter& operator=( ResultFormatter&& ) = delete;
};
#endif