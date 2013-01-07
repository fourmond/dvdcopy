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

  /// And a DVDFileData for output purposes
  const DVDFileData * dat;

  DVDFile(dvd_file_t * f, const DVDFileData * d);

public:

  /// Reads a given number of blocks at the given offset, and returns
  /// the number of blocks read, or -1 if an error occurred.
  virtual int readBlocks(int offset, int blocks, unsigned char * dest) = 0;

  /// Returns the size of the file in blocks
  int fileSize();


  /// Opens the given file. This returns something that should be
  /// freed with delete, and it can possibly return NULL, if opening
  /// went wrong.
  ///
  /// It actually returns a subclass of DVDFile, depending on the domain.
  ///
  /// @todo It still needs a reader argument, while it would be much
  /// nicer to make it part of DVDFileData ?
  static DVDFile * openFile(dvd_reader_t * reader, const DVDFileData * dat);

  virtual ~DVDFile();

  /// This functions reads @a blocks of blocks starting at @a start,
  /// by reads of @a steps block and runs the given functions upon
  /// successful reads and failed reads.
  void walkFile(int start, int blocks, int steps, 
                void (*successfulRead)(int offset, int nb, 
                                       unsigned char * buffer,
                                       const DVDFileData * dat,
                                       void * ptr),
                void (*failedRead)(int offset, int nb, 
                                   const DVDFileData * dat,
                                   void * ptr),
                void * ptr);
};



#endif
