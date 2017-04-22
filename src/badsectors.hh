/**
    \file badsectors.hh
    Storage and handling of bad sectors
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

#ifndef __BADSECTORS_H
#define __BADSECTORS_H

class DVDFileData;

/// This class represents the whole set of bad sectors in a DVDs
class BadSectorsFile {
protected:
  
  /// The list of bad sectors, indexed using the file name returned by
  /// DVDFileData::fileName()
  std::map<std::string, std::set<int> > badSectors;


  /// The file name
  std::string fileName;


  /// Marks the given sectors as bad sectors
  void markBadSectors(const std::string & file, int pos, int nb);

  /// Marks the given sectors as good sectors
  void clearBadSectors(const std::string & file, int pos, int nb);

public:

  /// Constructs a file
  BadSectorsFile(const std::string & file);

  /// Reads the list of bad sectors from the file
  void readBadSectors();

  /// Write out to the bad sector file
  void writeOut(FILE * out = NULL);

  /// Marks the given sectors as bad sectors
  void markBadSectors(const DVDFileData * file, int pos, int nb);

  /// Marks the given sectors as good sectors
  void clearBadSectors(const DVDFileData * file, int pos, int nb);

  /// Returns the bad sectors for the given file
  std::set<int> badSectorsForFile(const DVDFileData * file);

  /// Clears the bad sectors file
  void clear();

};




#endif
