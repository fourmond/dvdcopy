/** 
    \file main.c
    main file for dvdcopy
    Copyright Vincent Fourmond, 2006, 2008, 2011
 
    This is dvdcopy, a wrapper around libreaddvd facilities for
    reading DVDs.
  
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "headers.hh"
#include "dvdcopy.hh"

int main(int argc, char ** argv)
{
  DVDCopy dvd;
  // Hmmm
  dvd.copy(argv[1], argv[2]);
  return 0;
}
