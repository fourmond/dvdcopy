/**
    \file dvdoutfile.hh
    The DVDOutFile class, for output files
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

#ifndef __DVDOUTFILE_H
#define __DVDOUTFILE_H

/// Handles writing output files.
class DVDOutFile {
  /// Output file descriptor
  int fd;
  
  /// Output directory
  std::string outputDirectory;

  /// Title number
  int title;

  /// Title domain
  dvd_read_domain_t domain;

  /// Current sector (one DVD sector is 2048 bytes)
  int sector;

  /// Returns the numbered base file
  std::string makeFileName(int number = -1) const;

  /// Returns the full output file name 
  std::string outputFileName(int number = -1) const;

  /// Ensures the file descriptor is opened to the correct
  /// file. Handles file changing correctly.
  void openFile();

public:

  /// Creates and opens an output file.
  DVDOutFile(const char * output_dir, int title, 
             dvd_read_domain_t domain);

  /// Write sectors. \p number is the number of sectors, not the
  /// number of bytes.
  void writeSectors(const char * data, size_t number);

  /// Closes the output file
  void closeFile();

  /// Returns the current file name (including the VIDEO_TS bit, but
  /// not the output directory)
  std::string currentOutputName() const;

  /// Skip \p number sectors
  void skipSectors(size_t number);

  /// Returns the number of sectors already present in the output
  /// file.
  size_t fileSize() const;

  /// Seeks to the given sector:
  void seek(int sector);

  ~DVDOutFile();
};



#endif
