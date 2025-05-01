#include "opcda_cli.h"

int main( int argc, char* argv[] )
{
  CLIOptions opts;
  if ( !parseArguments( argc, argv, opts ) )
  {
    printHelp();
    return 1;
  }
  return runCLI( opts );
}
