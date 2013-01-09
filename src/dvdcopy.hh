/**
    \file dvdcopy.hh
    The DVDCopy class
    Copyright 2011 by Vincent Fourmond

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

#ifndef __DVDCOPY_H
#define __DVDCOPY_H

#include "dvdreader.hh"

/// Class representing a series of consecutive bad sectors.
///
/// @todo Write to- and from- string methods.
class BadSectors {
public:

  /// The underlying DVD file (ie something in the files list)
  const DVDFileData * file;
    
  /// The starting sector
  int start;

  /// The number of bad sectors;
  int number;

  BadSectors(const DVDFileData * f, int s, int n) :
    file(f), start(s), number(n) {;}

  /// Transform into a string
  std::string toString() const;

  /// If the given bad sector is next to this one, appends it and
  /// return true, else return false
  bool tryMerge(const BadSectors & follower);
};


/// Handles the actual copying job, from a source to a target.
class DVDCopy {
  /// Copies one file.
  ///
  /// If specified, the @a start and @a nb parameters define the
  /// starting sector and number of sectors to be read.
  ///
  /// @a readNumber sets the number of sectors to read in one
  /// go. Probably, for difficult cases, reading one-by-one may
  /// improve the usefulness ?
  ///
  /// it returns the number of skipped sectors.
  int copyFile(const DVDFileData * dat, int start = 0, 
               int nb = -1, int readNumber = -1);

  /// The DVD device we're reading
  dvd_reader_t * reader;
  
  /// The target directory.
  std::string targetDirectory;

  /// A list of bad sectors that were skipped upon reading.
  /// In the hope to be read again...
  FILE * badSectors;

  /// Writes a bad sector list to the bad sectors file (unless
  /// dontWrite is true), and add them to the badSectors list (in any
  /// case).
  void registerBadSectors(const DVDFileData * dat, 
                          int beg, int size, 
                          bool dontWrite = false);


  /// sets up the reader and gets the list of files, and sets up the
  /// target, creating the target directories if necessary.
  void setup(const char * source, const char * target);

  /// The underlying files of the source
  std::vector<DVDFileData *> files;

  /// The list of bad sectors, either read from the bad sectors file
  /// or directly populated registerBadSectors
  std::vector<BadSectors> badSectorsList;

  /// reads the bad sectors from the bad sectors file
  void readBadSectors();

  /// The name for the bad sectors file. It is constructed from the
  /// target if empty.
  std::string badSectorsFileName;


  /// Opens the bad sectors file with the given mode, if not open already.
  ///
  /// @todo There should be a way to track the mode last used in order
  /// not to try to read from a descriptor open for writing.
  void openBadSectorsFile(const char * mode);

  /// Finds the index of the file referred to by the title, domain,
  /// number triplet. Returns -1 if not found.
  int findFile(int title, dvd_read_domain_t domain, int number);

public:

  DVDCopy();

  /// Sets the bad sectors file name
  void setBadSectorsFileName(const char * file);

  /// Copies from source device to destination directory. The target
  /// directory should probably not exist.
  void copy(const char * source, const char * dest);

  /// Does a second pass, reading a bad sector files
  void secondPass(const char * source, const char * dest);

  /// Scans the source for bad sectors and make a bad sector list
  void scanForBadSectors(const char * source, 
                         const char * badSectorsFileName);

  ~DVDCopy();
};



#endif
