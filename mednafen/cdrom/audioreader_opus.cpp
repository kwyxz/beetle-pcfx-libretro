/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../mednafen.h"
#include "audioreader.h"
#include "audioreader_opus.h"

// OPUS SUPPORT NOT DONE YET!!!
/*

 (int64)op_pcm_total() * 44100 / 48000

 resampling vs seek, filter delay, etc. to consider
*/

static size_t iop_read_func(void *ptr, size_t size, size_t nmemb, void *user_data)
{
   Stream *fw = (Stream*)user_data;

   if(!size)
      return(0);

   return fw->read(ptr, size * nmemb, false) / size;
}

static int iop_seek_func(void *user_data, opus_int64 offset, int whence)
{
   Stream *fw = (Stream*)user_data;

   fw->seek(offset, whence);
   return(0);
}

static int iop_close_func(void *user_data)
{
   Stream *fw = (Stream*)user_data;

   fw->close();
   return(0);
}

static opus_int64 iop_tell_func(void *user_data)
{
   Stream *fw = (Stream*)user_data;

   return fw->tell();
}

/* Error strings copied from libopusfile header file comments. */
static const char *op_errstring(int error)
{
 static const struct
 {
  int code;
  const char *str;
 } error_table[] =
 {
  { OP_EREAD, "OP_EREAD: An underlying read, seek, or tell operation failed when it should have succeeded." },
  { OP_EFAULT, "OP_EFAULT: A NULL pointer was passed where one was unexpected, or an internal memory allocation failed, or an internal library error was encountered." },
  { OP_EIMPL, "OP_EIMPL: The stream used a feature that is not implemented, such as an unsupported channel family." },
  { OP_EINVAL, "OP_EINVAL: One or more parameters to a function were invalid." },
  { OP_ENOTFORMAT, "OP_ENOTFORMAT: A purported Ogg Opus stream did not begin with an Ogg page, or a purported header packet did not start with one of the required strings, \"OpusHead\" or \"OpusTags\"." },
  { OP_EBADHEADER, "OP_EBADHEADER: A required header packet was not properly formatted, contained illegal values, or was missing altogether." },
  { OP_EVERSION, "OP_EVERSION: The ID header contained an unrecognized version number." },
  { OP_EBADPACKET, "OP_EBADPACKET: An audio packet failed to decode properly." },
  { OP_EBADLINK, "OP_EBADLINK: We failed to find data we had seen before, or the bitstream structure was sufficiently malformed that seeking to the target destination was impossible." },
  { OP_ENOSEEK, "OP_ENOSEEK: An operation that requires seeking was requested on an unseekable stream." },
  { OP_EBADTIMESTAMP, "OP_EBADTIMESTAMP: The first or last granule position of a link failed basic validity checks." },
 };

 for(unsigned i = 0; i < sizeof(error_table) / sizeof(error_table[0]); i++)
 {
  if(error_table[i].code == error)
  {
   return _(error_table[i].str);
  }
 }

 return _("Unknown");
}

OggOpusReader::OggOpusReader(Stream *fp) : fw(fp)
{
 OpusFileCallbacks cb;
 int error = 0;

 memset(&cb, 0, sizeof(cb));
 cb.read_func = iop_read_func;
 cb.seek_func = iop_seek_func;
 cb.close_func = iop_close_func;
 cb.tell_func = iop_tell_func;

 fp->seek(0, SEEK_SET);

 if(!(opfile = op_open_callbacks((void*)fp, &cb, NULL, 0, &error)))
 {
  switch(error)
  {
     default:
        throw MDFN_Error(0, _("opusfile: error code: %d(%s)", error, op_errstring(error)));
        break;

     case OP_ENOTFORMAT:
        throw(0);
        break;
  }
 }
}

OggOpusReader::~OggOpusReader()
{
 op_free(opfile);
}

int64 OggOpusReader::Read_(int16 *buffer, int64 frames)
{
 int16 *tr_buffer = buffer;
 int64 tr_count = frames * 2;

 while(tr_count > 0)
 {
  int64 didread = op_read(opfile, tr_buffer, tr_count, NULL);

  if(didread == 0)
   break;

  tr_buffer += didread * 2;
  tr_count -= didread * 2;
 }

 return(frames - (tr_count / 2));
}

bool OggOpusReader::Seek_(int64 frame_offset)
{
 op_pcm_seek(opfile, frame_offset);
 return(true);
}

int64 OggOpusReader::FrameCount(void)
{
 return(op_pcm_total(pvfile, -1));
}
