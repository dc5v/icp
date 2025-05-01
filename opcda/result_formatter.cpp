#include "result_formatter.hpp"

ResultFormatter& ResultFormatter::getInstance()
{
  static ResultFormatter instance;
  return instance;
}