/*
 * Copyright 2013 Stanford University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  * Object representation of the parameter field.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#include "src/shared/parameter.h"

using boost::tokenizer;
using boost::char_separator;


namespace nimbus {

Parameter::Parameter() {
}

Parameter::Parameter(const SerializedData& ser_data)
  : ser_data_(ser_data) {
}

Parameter::Parameter(const Parameter& other)
  : ser_data_(other.ser_data_) {
}


Parameter::~Parameter() {
}


SerializedData Parameter::ser_data() {
  return ser_data_;
}

void Parameter::set_ser_data(SerializedData ser_data) {
  ser_data_ = ser_data;
}


std::string Parameter::ToNetworkData() {
  std::string str;
  str += ser_data_.ToNetworkData();
  return str;
}

Parameter& Parameter::operator= (const Parameter& right) {
  ser_data_ = right.ser_data_;
  return *this;
}

}  // namespace nimbus

