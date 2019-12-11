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

  class FileMatcher
  {
  private:
    FileMatcher();

  public:

    static void mergeFiles(std::vector<boost::filesystem::path> files, const std::string& targetFileName)
    {
      std::ofstream target(targetFileName);
      for (const boost::filesystem::path & f : files)
        {
          if (f.string() == targetFileName)
            {
              std::string excString = "The (target) file named: " + targetFileName + " already exists. Please delete it before moving forward!";
              std::cout << excString << std::endl;
              throw std::runtime_error(excString);
            }
          std::cout << f.string() << std::endl;
          std::ifstream fle(f.string());
          if (!fle.is_open())
            {
              throw std::runtime_error("Input file" + f.string() + " could not be opened.");
            }
          else if (!target.is_open()) {
              throw std::runtime_error("Target file: " + targetFileName + " could not be opened.");
            }
          else
            {
              if (fle.rdbuf()->in_avail() > 0)
                target << fle.rdbuf();
              else
                std::cout << "Empty file: " << f.string() << std::endl;
            }
        }
    }

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
