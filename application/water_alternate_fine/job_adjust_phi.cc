/* Copyright 2013 Stanford University.
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
 * This file contains the job that modifies levelset and particles. This job
 * should be spawned after calling advance time step forces and before
 * adjusting phi with sources.
 *
 * This job needs only phi.
 *
 * Author: Chinmayee Shah <chinmayee.shah@stanford.edu>
 */

#include "application/water_alternate_fine/app_utils.h"
#include "application/water_alternate_fine/job_adjust_phi.h"
#include "application/water_alternate_fine/physbam_include.h"
#include "application/water_alternate_fine/physbam_utils.h"
#include "application/water_alternate_fine/water_driver.h"
#include "application/water_alternate_fine/water_example.h"
#include "application/water_alternate_fine/water_sources.h"
#include "data/physbam/physbam_data.h"
#include "shared/dbg.h"
#include "shared/nimbus.h"
#include <sstream>
#include <string>

namespace application {

JobAdjustPhi::JobAdjustPhi(nimbus::Application *app) {
    set_application(app);
};

nimbus::Job* JobAdjustPhi::Clone() {
    return new JobAdjustPhi(application());
}

void JobAdjustPhi::Execute(nimbus::Parameter params, const nimbus::DataArray& da) {
    dbg(APP_LOG, "Executing modify levelset job\n");

    InitConfig init_config;
    T dt;
    std::string params_str(params.ser_data().data_ptr_raw(),
                           params.ser_data().size());
    LoadParameter(params_str, &init_config.frame, &init_config.time, &dt);
    dbg(APP_LOG, "Frame %i in modify levelset job\n", init_config.frame);

    const int& frame = init_config.frame;
    const T& time = init_config.time;

    // initialize configuration and state
    PhysBAM::WATER_EXAMPLE<TV> *example;
    PhysBAM::WATER_DRIVER<TV> *driver;

    init_config.set_boundary_condition = false;
    InitializeExampleAndDriver(init_config, this, da, example, driver);

    // adjust phi with sources
    dbg(APP_LOG, "Adjust Phi ...\n");
    example->Adjust_Phi_With_Sources(time+dt);

    // save state
    example->Save_To_Nimbus(this, da, frame+1);

    // free resources
    DestroyExampleAndDriver(example, driver);

    dbg(APP_LOG, "Completed executing modify levelset job\n");
}

} // namespace application
