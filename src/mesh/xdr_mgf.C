// The libMesh Finite Element Library.
// Copyright (C) 2002-2016 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// C++ includes
#include <iomanip>

// Local includes
#include "libmesh/xdr_mgf.h"

namespace libMesh
{

// XdrMGF member functions
XdrMGF::~XdrMGF()
{
  this->fini();
}



void XdrMGF::fini()
{

#ifdef LIBMESH_HAVE_XDR

  if (mp_xdr_handle)
    {
      //libMesh::out << "Destroying XDR file handle." << std::endl;
      xdr_destroy(mp_xdr_handle);
    }

  //libMesh::out << "Deleting the file handle pointer." << std::endl;
  delete mp_xdr_handle;

  mp_xdr_handle = libmesh_nullptr;

#endif

  if (mp_fp)
    {
      //libMesh::out << "Closing file." << std::endl;
      std::fflush(mp_fp);
      std::fclose(mp_fp);
    }

  mp_fp = libmesh_nullptr;
}






void XdrMGF::init (XdrMGF::XdrIO_TYPE t, const char * fn, const char *, int)
{
  m_type=t;

  // Close old file if necessary
  if (mp_fp) this->fini();


  // Open file
  switch (m_type)
    {

#ifdef LIBMESH_HAVE_XDR

    case (XdrMGF::ENCODE):
    case (XdrMGF::DECODE):
      {
        mp_fp = fopen (fn, (m_type == ENCODE) ? "w" : "r");

        // Make sure the file is ready for use
        if (!mp_fp)
          libmesh_error_msg("XDR Error: Accessing file: " << fn << " failed.");

        // Create the XDR handle
        mp_xdr_handle = new XDR;
        xdrstdio_create(mp_xdr_handle,
                        mp_fp,
                        ((m_type == ENCODE) ? XDR_ENCODE : XDR_DECODE));

        break;
      }

#endif

    case (XdrMGF::R_ASCII):
      {
        mp_in.open(fn, std::ios::in);

        // Make sure it opened correctly
        if (!mp_in.good())
          libmesh_file_error(fn);

        break;
      }

    case (XdrMGF::W_ASCII):
      {
        mp_out.open(fn, std::ios::out);

        // Make sure it opened correctly
        if (!mp_out.good())
          libmesh_file_error(fn);

        break;
      }

    default:
      libmesh_error_msg("Unrecognized file access type!");
    }





  // Read/Write the file signature
  const int  bufLen = 12;
  char       buf[bufLen+1];

  switch (m_type)
    {

#ifdef LIBMESH_HAVE_XDR

    case (XdrMGF::ENCODE):
      {
        char * p = &buf[0];
        const LegacyXdrIO::FileFormat orig = this->get_orig_flag();

        std::ostringstream name;
        if (orig == LegacyXdrIO::DEAL)
          name << "DEAL 003:003";

        else if (orig == LegacyXdrIO::MGF)
          name << "MGF  002:000";

        else if (orig == LegacyXdrIO::LIBM)
          name << "LIBM " << this->get_num_levels();

        else
          libmesh_error_msg("Unknown orig " << orig);

        // Fill the buffer
        std::sprintf(&buf[0], "%s", name.str().c_str());

        xdr_string(mp_xdr_handle, &p, bufLen);  // Writes binary signature

        break;
      }

    case (XdrMGF::DECODE):
      {
        char * p = &buf[0];
        xdr_string(mp_xdr_handle, &p, bufLen); // Reads binary signature

        // Set the number of levels used in the mesh
        this->tokenize_first_line(p);

        break;
      }

#endif

    case (XdrMGF::W_ASCII):
      {
        const LegacyXdrIO::FileFormat orig = this->get_orig_flag();

        if (orig == LegacyXdrIO::DEAL)
          std::sprintf(&buf[0], "%s %03d:%03d", "DEAL", 3, 3);

        else if (orig == LegacyXdrIO::MGF)
          std::sprintf(&buf[0], "%s %03d:%03d", "MGF ", 2, 0);

        else if (orig == LegacyXdrIO::LIBM)
          std::sprintf(&buf[0], "%s %d", "LIBM", this->get_num_levels());

        mp_out << buf << '\n';

        break;
      }

    case (XdrMGF::R_ASCII):
      {

#ifdef __HP_aCC
        // weirdly, _only_ here aCC
        // is not fond of mp_in.getline()
        // however, using mp_in.getline()
        // further below is ok...
        std::string buf_buf;
        std::getline (mp_in, buf_buf, '\n');
        libmesh_assert_less_equal (buf_buf.size(), bufLen);

        buf_buf.copy (buf, std::string::npos);
#else

        // Here we first use getline() to grab the very
        // first line of the file into a char buffer.  Then
        // this line is tokenized to look for:
        // 1.) The name LIBM, which specifies the new Mesh style.
        // 2.) The number of levels in the Mesh which is being read.
        // Note that "buf" will be further processed below, here we
        // are just attempting to get the number of levels.
        mp_in.getline(buf, bufLen+1);

#endif

        // Determine the number of levels in this mesh
        this->tokenize_first_line(buf);

        break;
      }

    default:
      libmesh_error_msg("Unknown m_type" << m_type);
    }



  // If you are reading or decoding, process the signature
  if ((m_type == R_ASCII) || (m_type == DECODE))
    {
      char name[5];
      std::strncpy(name, &buf[0], 4);
      name[4] = '\0';

      if (std::strcmp (name, "DEAL") == 0)
        {
          this->orig_flag = LegacyXdrIO::DEAL; // 0 is the DEAL identifier by definition
        }
      else if (std::strcmp (name, "MGF ") == 0)
        {
          this->orig_flag = LegacyXdrIO::MGF; // 1 is the MGF identifier by definition
        }
      else if (std::strcmp (name, "LIBM") == 0)
        {
          this->orig_flag = LegacyXdrIO::LIBM; // the New and Improved XDA
        }

      else
        libmesh_error_msg("ERROR: No originating software can be determined for header string '" << name);
    }

}



int XdrMGF::dataBlk(int * array, int numvar, int size)
{
  int totalSize = numvar*size;

  switch (m_type)
    {

#ifdef LIBMESH_HAVE_XDR

    case (XdrMGF::DECODE):
    case (XdrMGF::ENCODE):
      {
        xdr_vector(mp_xdr_handle,
                   (char *) &array[0],
                   totalSize,
                   sizeof(int),
                   (xdrproc_t) xdr_int);
        break;
      }

#endif

    case (XdrMGF::W_ASCII):
      {
        for (int i=0; i<size; i++)
          {
            for (int j=0; j<numvar; j++)
              mp_out << array[i*numvar + j] << " ";

            mp_out << '\n';
          }

        mp_out.flush();
        break;
      }

    case (XdrMGF::R_ASCII):
      {
        libmesh_assert (mp_in.good());

        for (int i=0; i<size; i++)
          {
            for (int j=0; j<numvar; j++)
              {
                mp_in >> array[i*numvar + j];
              }

            mp_in.ignore(); // Read newline
          }

        break;
      }

    default:
      // Unknown access type
      libmesh_error_msg("Unknown m_type" << m_type);
    }

  return totalSize;
}



int XdrMGF::dataBlk(Real * array, int numvar, int size)
{
  int totalSize = numvar*size;

  // If this function is called by coord(),
  // numvar is the problem dimension, and
  // size is the number of nodes in the problem.

  //libMesh::out << "Total amount of data to be written: " << totalSize << std::endl;

  switch (m_type)
    {

#ifdef LIBMESH_HAVE_XDR

    case (XdrMGF::DECODE):
    case (XdrMGF::ENCODE):
      {
        // FIXME - this is probably broken for Real == long double
        // RHS
        xdr_vector(mp_xdr_handle,
                   (char *) &array[0],
                   totalSize,
                   sizeof(Real),
                   (xdrproc_t) xdr_REAL);
      }

#endif

    case (XdrMGF::W_ASCII):
      {
        // Save stream flags
        std::ios_base::fmtflags out_flags = mp_out.flags();

        // We will use scientific notation with a precision of 16
        // digits in the following output.  The desired precision and
        // format will automatically determine the width.
        mp_out << std::scientific
               << std::setprecision(16);

        for (int i=0; i<size; i++)
          {
            for (int j=0; j<numvar; j++)
              mp_out << array[i*numvar + j] << " \t";

            mp_out << '\n';
          }

        // Restore stream flags
        mp_out.flags(out_flags);

        mp_out.flush();
        break;
      }

    case (XdrMGF::R_ASCII):
      {
        libmesh_assert (mp_in.good());

        for (int i=0; i<size; i++)
          {
            libmesh_assert (mp_in.good());

            for (int j=0; j<numvar; j++)
              mp_in >> array[i*numvar + j];

            mp_in.ignore(); // Read newline
          }

        break;
      }

    default:
      // Unknown access type
      libmesh_error_msg("Unknown m_type" << m_type);
    }

  return totalSize;
}

} // namespace libMesh
