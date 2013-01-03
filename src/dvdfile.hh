/**
    \file dvdfile.hh
    The DVDFile class and its subclasses, handling reading files
    Copyright 2012 by Vincent Fourmond

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

#ifndef __DVDFILE_H
#define __DVDFILE_H

class DVDFileData;

/// Handles reading input files.
class DVDFile {
protected:

  /// The underlying dvdfile_t object
  dvd_file_t * file;

  DVDFile(dvd_file_t * f);

public:

  /// Reads a given number of blocks at the given offset, and returns
  /// the number of blocks read, or -1 if an error occurred.
  virtual int readBlocks(int blocks, int offset, unsigned char * dest);

  /// Returns the size of the file in blocks
  int fileSize();


  /// Opens the given file. This returns something that should be
  /// freed with delete, and it can possibly return NULL, if opening
  /// went wrong.
  ///
  /// It actually returns a subclass of DVDFile, depending on the domain.
  static DVDFile * openFile(DVDFileData * dat);

  virtual ~DVDFile();
};



#endif
