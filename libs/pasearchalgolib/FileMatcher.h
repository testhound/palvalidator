// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef FILEMATCHER_H
#define FILEMATCHER_H

#include <boost/filesystem.hpp>
#include <iostream>

namespace mkc_searchalgo
{
  static int wildcmp(const char *wild, const char *string) {

    const char *cp = nullptr, *mp = nullptr;

    while ((*string) && (*wild != '*')) {
        if ((*wild != *string) && (*wild != '?')) {
            return 0;
          }
        wild++;
        string++;
      }

    while (*string) {
        if (*wild == '*') {
            if (!*++wild) {
                return 1;
              }
            mp = wild;
            cp = string+1;
          } else if ((*wild == *string) || (*wild == '?')) {
            wild++;
            string++;
          } else {
            wild = mp;
            string = cp++;
          }
      }

    while (*wild == '*') {
        wild++;
      }
    return !*wild;
  }

  class FileMatcher {
  private:
    FileMatcher();
  public:
    static std::vector<boost::filesystem::path> getFiles(const std::string& pathStr, const std::string& matchExpression)
    {
      std::vector<boost::filesystem::path> ret;

      boost::filesystem::path targetDir(pathStr);
      boost::filesystem::recursive_directory_iterator iter(targetDir), eod;

      for(const boost::filesystem::path & p : iter)
        {
          //std::cout << p << std::endl;
          if (wildcmp(matchExpression.c_str(), p.c_str()))
            {
              //std::cout << "matching expression." << std::endl;
              ret.push_back(p);
            }
        }
      return ret;
    }
  };



}

#endif // FILEMATCHER_H
