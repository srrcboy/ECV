// Copyright 2011 Ethan Eade. All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:

//    1. Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.

//    2. Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY ETHAN EADE ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ETHAN EADE OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

// The views and conclusions contained in the software and
// documentation are those of the authors and should not be
// interpreted as representing official policies, either expressed or
// implied, of Ethan Eade.

#ifndef ECV_IO_PGM_HPP
#define ECV_IO_PGM_HPP

#include <ecv/image.hpp>
#include <ecv/io_pnm.hpp>

#include <stdint.h>
#include <iostream>
#include <vector>
#include <cassert>

namespace ecv {
    
    bool pgm_read(std::istream& in, Image<uint8_t>& im)
    {
        pnm_io::Header header;
        if (!pnm_io::read_header(in, header))
            return false;

        if (header.fmt != '2' && header.fmt != '5')
            return false;
        
        if (header.width < 0 || header.height < 0)
            return false;
        
        if (header.maxval < 1 || header.maxval > 65535)
            return false;

        if (header.fmt == '5') {
            im.resize(header.width, header.height);
            if (header.maxval == 255) {
                for (int i=0; i<im.height(); ++i) {
                    in.read((char*)im[i], im.width());
                    if (in.gcount() != im.width())
                        return false;
                }
                return true;
            }
            double factor = 255.0 / header.maxval;
            if (header.maxval > 255) {
                std::vector<uint16_t> rowbuf(im.width());
                for (int i=0; i<im.height(); ++i) {
                    in.read((char*)&rowbuf[0], im.width()*2);
                    if (in.gcount() != im.width()*2)
                        return false;
                    uint8_t *imi = im[i];
                    for (int j=0; j<im.width(); ++j)
                        imi[j] = (uint8_t)(rowbuf[j] * factor + 0.5);
                }
            } else {
                for (int i=0; i<im.height(); ++i) {
                    in.read((char*)im[i], im.width());
                    if (in.gcount() != im.width())
                        return false;
                    uint8_t * imi = im[i];
                    for (int j=0; j<im.width(); ++j)
                        imi[j] = (uint8_t)(imi[j] * factor + 0.5);
                }
            }
        } else if (header.fmt == '2') {
            double factor = 255.0 / header.maxval;
            for (int i=0; i<im.height(); ++i) {
                uint8_t * imi = im[i];
                for (int j=0; j<im.width(); ++j) {
                    int val;
                    if (!(in >> val))
                        return false;                    
                    imi[j] = (uint8_t)(val * factor + 0.5);
                }
            }
        }
        return true;
    }

    void pgm_write(const Image<uint8_t>& im, std::ostream& out)
    {
        out << "P5\n"
            << im.width() << " " << im.height() << "\n"
            << "255\n";
        for (int i=0; i<im.height(); ++i)
            out.write((const char*)im[i], im.width());
    }

    void pgm_write(const Image<uint16_t>& im, std::ostream& out)
    {
        out << "P5\n"
            << im.width() << " " << im.height() << "\n"
            << "65535\n";
        for (int i=0; i<im.height(); ++i)
            out.write((const char*)im[i], im.width()*2);
    }   
    
}

#endif
