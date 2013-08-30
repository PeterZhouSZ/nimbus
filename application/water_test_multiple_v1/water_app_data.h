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
 * Data used by application.
 *
 * Author: Chinmayee Shah <chinmayee.shah@stanford.edu>
 */

#ifndef NIMBUS_APPLICATION_WATER_TEST_MULTIPLE_V1_WATER_APP_DATA_H_
#define NIMBUS_APPLICATION_WATER_TEST_MULTIPLE_V1_WATER_APP_DATA_H_

#include "shared/nimbus.h"
#include "physbam_include.h"

namespace water_app_data {

    typedef ::PhysBAM::VECTOR<float, 2> TVF2;
    typedef float TF;

    /* Face array for storing quantities like face velocities.
     */
    template <class TV>
        class FaceArray : public ::nimbus::Data {

            private:

                typedef typename TV::SCALAR T;
                typedef typename TV::template REBIND<int>::TYPE TV_INT;
                typedef typename ::PhysBAM::GRID_ARRAYS_POLICY<GRID<TV> >
                    ::FACE_ARRAYS T_FACE_ARRAYS_SCALAR;
                typedef typename ::PhysBAM::GRID<TV> T_GRID;
                typedef typename 
                    ::PhysBAM::ARRAY<T, ::PhysBAM::FACE_INDEX<TV::dimension> >
                    T_FACE_ARRAY;

                int size_;

            public:

                //TODO(chinmayee): reimplement this so that driver does not
                //need to be included in data. this is a mess right now.
                //void Advection (WaterDriver<TV> *driver,
                //        NonAdvData<TV, T> *sim_data);

                FaceArray(int size);
                virtual void create();
                virtual ::nimbus::Data* clone();
                virtual int get_debug_info();

                // debug information
                int id_debug;

                // physbam structures and methods
                T_GRID *grid;
                T_FACE_ARRAY *data;
        };

} // namespace water_app_data

#endif // NIMBUS_APPLICATION_WATER_TEST_MULTIPLE_V1_WATER_APP_DATA_H_
