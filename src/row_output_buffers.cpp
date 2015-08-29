/*
  row_output_buffers.cpp

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 - 2015 Qore Technologies

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sybase.h"

#include <assert.h>

#include <memory>
#include "row_output_buffers.h"
#include "utils.h"

output_value_buffer::output_value_buffer(unsigned size) :
    indicator(0), value(0), value_len(0) {
    if (size < 7) size = 7; // ensure at least 8 bytes are allocated. hmmm, why?
    value = new char[size + 1]; // terminator for strings
    value[size] = 0; // this is required since the result values from sybase or
                     // freetds don't necesary have trailing \0 and qore calls
                     // strlen() everywhere...
}

output_value_buffer::~output_value_buffer() {
   delete [] value;
}

row_output_buffers::~row_output_buffers() {
    reset();
}

void row_output_buffers::reset() {
    ss::delete_container(m_buffers);
    m_buffers.clear();
}

output_value_buffer * row_output_buffers::insert(size_t size) {
    std::auto_ptr<output_value_buffer> out(new output_value_buffer(size));
    m_buffers.push_back(out.get());
    return out.release();
}
