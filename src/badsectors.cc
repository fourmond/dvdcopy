/**
    \file badsectors.cc
    Implementation of the B class
    Copyright 2017 by Vincent Fourmond

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "headers.hh"
#include "badsectors.hh"

#include "dvdreader.hh"


#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/time.h>

// use of regular expressions !
#include <regex.h>


BadSectorsFile::BadSectorsFile(const std::string & file) :
  fileName(file)
{
  readBadSectors();
}

/// Write out to the bad sector file
void BadSectorsFile::writeOut(FILE * out)
{
  bool shouldClose = false;
  if(! out) {
    if(fileName.empty())
      throw std::runtime_error("Can't write unnamed bad sectors file");
    out = fopen(fileName.c_str(), "w");
    shouldClose = true;
  }
  if(! out)
    throw std::runtime_error("Error opening file");
  for(auto it = badSectors.begin(); it != badSectors.end(); ++it) {
    const std::string & file = it->first;
    const std::set<int> & lst = it->second;
    int first = -1, last = -1;
    for(int cur : lst) {
      if(first < 0) {
        first = cur;
        last = cur;
      }
      else {
        if(last < cur -1) {
          fprintf(out, "%s: %d (%d)\n",
                  file.c_str(),
                  first, last-first + 1);
          first = cur;
          last = cur;
        }
        else
          last = cur;
      }
      fflush(out);
    }
    fprintf(out, "%s: %d (%d)\n",
            file.c_str(),
            first, last-first + 1);
    fflush(out);
  }
  if(shouldClose)
    fclose(out);
}

void BadSectorsFile::readBadSectors()
{
  FILE * in = fopen(fileName.c_str(), "r");
  if(! in)
    return;                     // nothing to read;

  char buffer[1024];
  regex_t re;
  regmatch_t matches[6];
  { 
    int er = regcomp(&re, "([^:]+): *([0-9]+,[0-9]+,[0-9]+)? *"
                     "([0-9]+) *\\(([0-9]+)\\)",
                     REG_EXTENDED);
    if(er) {
      regerror(er, &re, buffer, sizeof(buffer));
      fprintf(stderr, "Error building the line regexp: %s", buffer);
      return;
    }
  }

  int line = 0;
  while(! feof(in)) {
    if(! fgets(buffer, sizeof(buffer), in))
      break;
    ++line;
    // fprintf(stderr, "line %d: '%s'", line, buffer);
    int status = regexec(&re, buffer, sizeof(matches)/sizeof(regmatch_t),
                         matches, 0);
    if(status) {
      fprintf(stderr, "error parsing line %d: '%s'", line, buffer);
    }
    else {
      // Make all substrings NULL-terminated:
      for(int i = 1; i < sizeof(matches)/sizeof(regmatch_t); i++) {
        if(matches[i].rm_so >= 0)
          buffer[matches[i].rm_eo] = 0;
      }
      
      // No validation whatsoever, but all groups should be here anyway !
      std::string file = buffer + matches[1].rm_so;
      int beg = atoi(buffer + matches[3].rm_so);
      int size = atoi(buffer + matches[4].rm_so);

      markBadSectors(file, beg, size);
    }
  }
}

void BadSectorsFile::markBadSectors(const std::string & file,
                                    int pos, int nb)
{
  std::set<int> & tgt = badSectors[file];
  while(nb > 0) {
    tgt.insert(pos++);
    --nb;
  }
}

void BadSectorsFile::markBadSectors(const DVDFileData * file,
                                    int pos, int nb)
{
  markBadSectors(file->fileName(), pos, nb);
}

void BadSectorsFile::clearBadSectors(const std::string & file,
                                     int pos, int nb)
{
  std::set<int> & tgt = badSectors[file];
  while(nb > 0) {
    tgt.erase(pos++);
    --nb;
  }
}

void BadSectorsFile::clearBadSectors(const DVDFileData * file,
                                    int pos, int nb)
{
  clearBadSectors(file->fileName(), pos, nb);
}

std::set<int> BadSectorsFile::badSectorsForFile(const DVDFileData * file)
{
  auto it = badSectors.find(file->fileName());
  if(it != badSectors.end())
    return it->second;
  else
    return std::set<int>();
}

void BadSectorsFile::clear()
{
  badSectors.clear();
}
