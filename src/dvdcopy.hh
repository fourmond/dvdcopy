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


/// This class represents the total progress for a copy (or re-read)
/// operation
class Progress {
protected:

  /// This class represents the current progress for a given DVDFileData
  class FileProgress {
  public:
    /// The total number of sectors to read in this file
    int totalSectors;
    
    /// The number of sectors already read in this file
    int sectorsDone;
    
    /// The number of skipped sectors in this file
    int skippedSectors;

  };

  std::map<const DVDFileData *, FileProgress> progresses;

  int totalSectors;
  int sectorsDone;
  int totalSkipped;

  /// Time at which the reading process started.
  struct timeval startTime;


public:

  /// Sets up a progress report for a copy operation
  void setupForCopying(const std::vector<DVDFileData * > & files);

  void finishedFile(const DVDFileData * file);

  /// Advance the given file by that many sectors
  void successfulRead(const DVDFileData * file, int nb);

  /// Advance the given file by that many skipped sectors
  void failedRead(const DVDFileData * file, int nb);

  /// Writes the current progress (for the given file, and for all)
  void writeCurrentProgress(const DVDFileData * file) const;
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

  /// The source device
  std::string sourceDevice;
  
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

  /// Closes the bad sectors file !
  void closeBadSectorsFile();

  /// Finds the index of the file referred to by the title, domain,
  /// number triplet. Returns -1 if not found.
  int findFile(int title, dvd_read_domain_t domain, int number);

  /// In principle, backup files are good, but in practice, they seem
  /// to be used to implement copy-protection schemes, so they should
  /// be disabled by default. This is the purpose of this flag.
  bool skipBUP;

  /// Analyse a given IFO file to extract the relevant sector
  /// informations. It returns the number of sectors in the IFO file
  /// and in the "title".
  ///
  /// A device must be setup already.
  void extractIFOSizes(const DVDFileData * file, 
                       int * ifoSectors,
                       int * titleSectors = NULL);

  Progress overallProgress;

public:

  DVDCopy();

  /// Sets the bad sectors file name
  void setBadSectorsFileName(const char * file);

  /// Copies from source device to destination directory. The target
  /// directory should probably not exist.
  void copy(const char * source, const char * dest);


  /// Splice IFO files from BUP files, while keeping the _nb_ first
  /// sectors.
  void spliceIFO(const char * source, const char * dest, int nb);

  /// Does a second pass, reading a bad sector files
  void secondPass(const char * source, const char * dest);

  /// Scans the source for bad sectors and make a bad sector list
  void scanForBadSectors(const char * source, 
                         const char * badSectorsFileName);


  /// Scans the source's IFO files for information.
  void scanIFOs(const char * source);

  /// Attempts to eject the drive.
  void ejectDrive();

  /// Number of sectors read in one go (in the normal operations)
  int sectorsRead;


  ~DVDCopy();
};



#endif
