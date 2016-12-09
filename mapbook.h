/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004-2016 Antonio Diaz Diaz.

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

#ifdef DDRESCUE_USE_DVDREAD
extern "C" {
#include <dvdread/dvd_reader.h>
}
#endif


class Mapbook : public Mapfile
  {
  const long long offset_;		// outfile offset (opos - ipos);
  long long mapfile_isize_;
  Domain & domain_;			// rescue domain
  uint8_t *iobuf_base, *iobuf_;		// iobuf is aligned to page and hardbs
  const int hardbs_, softbs_;
  const int iobuf_size_;
  std::string final_msg_;
  int final_errno_;
  long um_t1, um_t1s;			// variables for update_mapfile
  bool mapfile_exists_;

  bool save_mapfile( const char * const name );
  bool emergency_save();

  Mapbook( const Mapbook & );		// declared as private
  void operator=( const Mapbook & );	// declared as private

public:
  Mapbook( const long long offset, const long long isize,
           Domain & dom, const char * const mapname,
           const int cluster, const int hardbs, const bool complete_only );
  ~Mapbook() { delete[] iobuf_base; }

  bool update_mapfile( const int odes = -1, const bool force = false );

  const Domain & domain() const { return domain_; }
  uint8_t * iobuf() const { return iobuf_; }
  uint8_t * iobuf_aux() const	// hardbs-sized buffer for verify_on_error
    { return iobuf_ + iobuf_size_; }
  int iobuf_size() const { return iobuf_size_; }
  int hardbs() const { return hardbs_; }
  int softbs() const { return softbs_; }
  long long offset() const { return offset_; }
  const std::string & final_msg() const { return final_msg_; }
  int final_errno() const { return final_errno_; }
  bool mapfile_exists() const { return mapfile_exists_; }
  long long mapfile_isize() const { return mapfile_isize_; }

  void final_msg( const std::string & msg, const int e = 0 )
    { final_msg_ = msg; final_errno_ = e; }

  void truncate_domain( const long long end )
    { domain_.crop_by_file_size( end ); }
  };


struct Fb_options
  {
  std::string filltypes;
  bool ignore_write_errors;
  bool write_location_data;

  Fb_options() : ignore_write_errors( false ), write_location_data( false ) {}

  bool operator==( const Fb_options & o ) const
    { return ( filltypes == o.filltypes &&
               ignore_write_errors == o.ignore_write_errors &&
               write_location_data == o.write_location_data ); }
  bool operator!=( const Fb_options & o ) const
    { return !( *this == o ); }
  };


class Fillbook : public Mapbook, public Fb_options
  {
  long long filled_size;		// size already filled
  long long remaining_size;		// size to be filled
  long filled_areas;			// areas already filled
  long remaining_areas;			// areas to be filled
  int odes_;				// output file descriptor
  const bool synchronous_;
					// variables for show_status
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1;				// start, current times
  int oldlen;

  int fill_block( const Sblock & sb );
  int fill_areas();
  void show_status( const long long ipos, const char * const msg = 0,
                    bool force = false );

public:
  Fillbook( const long long offset, Domain & dom,
            const char * const mapname, const int cluster, const int hardbs,
            const Fb_options & fb_opts, const bool synchronous )
    : Mapbook( offset, 0, dom, mapname, cluster, hardbs, true ),
      Fb_options( fb_opts ),
      synchronous_( synchronous ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 ), oldlen( 0 )
      {}

  int do_fill( const int odes );
  bool read_buffer( const int ides );
  };


class Genbook : public Mapbook
  {
  long long finished_size, gensize;	// total recovered and generated sizes
  int odes_;				// output file descriptor
					// variables for show_status
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1;				// start, current times
  int oldlen;

  void check_block( const Block & b, int & copied_size, int & error_size );
  int check_all();
  void show_status( const long long ipos, const char * const msg = 0,
                    bool force = false );
public:
  Genbook( const long long offset, const long long isize,
           Domain & dom, const char * const mapname,
           const int cluster, const int hardbs )
    : Mapbook( offset, isize, dom, mapname, cluster, hardbs, false ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 ), oldlen( 0 )
      {}

  int do_generate( const int odes );
  };


inline bool block_is_zero( const uint8_t * const buf, const int size )
  {
  for( int i = 0; i < size; ++i ) if( buf[i] != 0 ) return false;
  return true;
  }


// Defined in genbook.cc
//
const char * format_time( const long t, const bool low_prec = false );

// Defined in io.cc
//
int readblock( const int fd, uint8_t * const buf, const int size,
               const long long pos );
#ifdef DDRESCUE_USE_DVDREAD
int readblock_dvdread( dvd_reader_t *dvd, uint32_t dvd_blocks, uint8_t * const buf, const int size,
                       const long long pos );
#endif
int writeblock( const int fd, const uint8_t * const buf, const int size,
                const long long pos );
bool interrupted();
void set_signals();
int signaled_exit();
