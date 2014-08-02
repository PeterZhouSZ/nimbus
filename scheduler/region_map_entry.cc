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
  * Each element in the region map which defines an unstructured region and
  * additional meta data for load balancing.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */


#include "scheduler/region_map_entry.h"

namespace nimbus {

RegionMapEntry::RegionMapEntry() {
}

RegionMapEntry::RegionMapEntry(const RegionMapEntry& other) {
  region_ = other.region_;
}

RegionMapEntry::~RegionMapEntry() {
}

RegionMapEntry& RegionMapEntry::operator= (const RegionMapEntry& right) {
  region_ = right.region_;
  return *this;
}

void RegionMapEntry::Grow(const GeometricRegion *region) {
  region_.AddRegion(region);
}

void RegionMapEntry::Shrink(const GeometricRegion *region) {
  region_.RemoveRegion(region);
}

int_dimension_t RegionMapEntry::CommonSurface(const GeometricRegion *region) {
  return region_.CommonSurface(region);
}

std::string RegionMapEntry::PrintRegion() {
  return region_.Print();
}

}  // namespace nimbus