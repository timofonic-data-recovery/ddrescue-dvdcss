/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004-2015 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

#include "block.h"
#include "blockbook.h"


namespace {

void input_pos_error( const long long pos, const long long isize )
  {
  char buf[128];
  snprintf( buf, sizeof buf,
            "Can't start reading at pos %lld.\n"
            "          Input file is only %lld bytes long.", pos, isize );
  show_error( buf );
  }

} // end namespace


bool Blockbook::save_blockfile( const char * const name )
  {
  std::remove( name );
  FILE * const f = std::fopen( name, "w" );
  if( f && write_blockfile( f, true ) && std::fclose( f ) == 0 )
    {
    char buf[80];
    snprintf( buf, sizeof buf, "Blockfile saved in '%s'\n", name );
    final_msg( buf );
    return true;
    }
  return false;
  }


bool Blockbook::emergency_save()
  {
  static bool first_time = true;
  static std::string home_name;
  const std::string dead_name( "ddrescue.bf" );

  if( filename() != dead_name && save_blockfile( dead_name.c_str() ) )
    return true;
  if( first_time )
    {
    first_time = false;
    const char * const p = std::getenv( "HOME" );
    if( p ) { home_name = p; home_name += '/'; home_name += dead_name; }
    }
  if( home_name.size() &&
      filename() != home_name && save_blockfile( home_name.c_str() ) )
    return true;
  show_error( "Emergency save failed." );
  return false;
  }


Blockbook::Blockbook( const long long offset, const long long isize,
                      Domain & dom, const char * const bfname,
                      const int cluster, const int hardbs,
                      const bool complete_only )
  : Blockfile( bfname ), offset_( offset ), blockfile_isize_( 0 ),
    domain_( dom ), hardbs_( hardbs ), softbs_( cluster * hardbs_ ),
    iobuf_size_( hardbs_ + softbs_ ),
    final_errno_( 0 ), ul_t1( 0 ), blockfile_exists_( false )
  {
  int alignment = sysconf( _SC_PAGESIZE );
  if( alignment < hardbs_ || alignment % hardbs_ ) alignment = hardbs_;
  if( alignment < 2 || alignment > 65536 ) alignment = 0;
  iobuf_ = iobuf_base = new uint8_t[ iobuf_size_ + alignment ];
  if( alignment > 1 )		// align iobuf for use with raw devices
    {
    const int disp = alignment - ( reinterpret_cast<long> (iobuf_) % alignment );
    if( disp > 0 && disp < alignment ) iobuf_ += disp;
    }

  if( isize > 0 )
    {
    if( domain_.pos() >= isize )
      { input_pos_error( domain_.pos(), isize ); std::exit( 1 ); }
    domain_.crop_by_file_size( isize );
    }
  if( filename() )
    {
    blockfile_exists_ = read_blockfile();
    if( blockfile_exists_ ) blockfile_isize_ = extent().end();
    }
  if( !complete_only ) extend_sblock_vector( isize );
  else domain_.crop( extent() );  // limit domain to blocks read from blockfile
  compact_sblock_vector();
  split_by_domain_borders( domain_ );
  if( sblocks() == 0 ) domain_.clear();
  }


// Writes periodically the blockfile to disc.
// Returns false only if update is attempted and fails.
//
bool Blockbook::update_blockfile( const int odes, const bool force )
  {
  if( !filename() ) return true;
  const int interval = 30 + std::min( 270L, sblocks() / 38 );	// 30s to 5m
  const long t2 = std::time( 0 );
  if( ul_t1 == 0 ) ul_t1 = t2;				// initialize
  if( !force && t2 - ul_t1 < interval ) return true;
  ul_t1 = t2;
  if( odes >= 0 ) fsync( odes );

  while( true )
    {
    errno = 0;
    if( write_blockfile( 0, true ) ) return true;
    if( verbosity < 0 ) return false;
    const int saved_errno = errno;
    std::fputc( '\n', stderr );
    char buf[80];
    snprintf( buf, sizeof buf, "Error writing blockfile '%s'", filename() );
    show_error( buf, saved_errno );
    std::fputs( "Fix the problem and press ENTER to retry,\n"
                "                     or E+ENTER for an emergency save and exit,\n"
                "                     or Q+ENTER to abort.\n", stderr );
    std::fflush( stderr );
    while( true )
      {
      tcflush( STDIN_FILENO, TCIFLUSH );
      const int c = std::tolower( std::fgetc( stdin ) );
      int tmp = c;
      while( tmp != '\n' && tmp != EOF ) tmp = std::fgetc( stdin );
      if( c == '\r' || c == '\n' || c == 'e' || c == 'q' )
        {
        if( c == 'q' || ( c == 'e' && emergency_save() ) )
          { if( !force ) std::fputs( "\n\n\n\n", stdout ); return false; }
        break;
        }
      }
    }
  }