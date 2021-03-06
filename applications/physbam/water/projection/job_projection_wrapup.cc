/* Copyright 2013 Stanford University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 vd* are met:
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
 * Author: Hang Qu <quhang@stanford.edu>
 */

#include <sstream>
#include <string>

#include "applications/physbam/water//app_utils.h"
#include "applications/physbam/water//physbam_utils.h"
#include "applications/physbam/water//water_driver.h"
#include "applications/physbam/water//water_example.h"
#include "src/shared/dbg.h"
#include "src/shared/nimbus.h"

#include "applications/physbam/water//projection/job_projection_wrapup.h"

namespace application {

JobProjectionWrapup::JobProjectionWrapup(nimbus::Application *app) {
  set_application(app);
};

nimbus::Job* JobProjectionWrapup::Clone() {
  return new JobProjectionWrapup(application());
}

void JobProjectionWrapup::Execute(nimbus::Parameter params,
                        const nimbus::DataArray& da) {
  dbg(APP_LOG, "Executing PROJECTION_WRAPUP job.\n");

  InitConfig init_config;
  init_config.set_boundary_condition = false;
  std::string params_str(params.ser_data().data_ptr_raw(),
                         params.ser_data().size());
  LoadParameter(params_str, &init_config);
  T dt = init_config.dt;

  // Assume time, dt, frame is ready from here.
  dbg(APP_LOG,
      "In PROJECTION: Initialize WATER_DRIVER/WATER_EXAMPLE"
      "(Frame=%d, Time=%f).\n",
      init_config.frame, init_config.time);

  PhysBAM::WATER_EXAMPLE<TV> *example;
  PhysBAM::WATER_DRIVER<TV> *driver;

  DataConfig data_config;
  data_config.SetFlag(DataConfig::VELOCITY);
  data_config.SetFlag(DataConfig::LEVELSET);
  data_config.SetFlag(DataConfig::DIVERGENCE);
  data_config.SetFlag(DataConfig::PSI_N);
  data_config.SetFlag(DataConfig::PSI_D);
  data_config.SetFlag(DataConfig::REGION_COLORS);
  data_config.SetFlag(DataConfig::PRESSURE);
  data_config.SetFlag(DataConfig::U_INTERFACE);
  data_config.SetFlag(DataConfig::INDEX_M2C);
  InitializeExampleAndDriver(init_config, data_config,
                             this, da, example, driver);

  dbg(APP_LOG, "Job PROJECTION_WRAPUP starts (dt=%f).\n", dt);

  {
    application::ScopeTimer scope_timer(name());
    driver->ProjectionWrapupImpl(this, da, dt);
  }

  // Write, free resources.
  example->Save_To_Nimbus(this, da, driver->current_frame + 1);
  DestroyExampleAndDriver(example, driver);

  dbg(APP_LOG, "Completed executing PROJECTION_WRAPUP job\n");
}

}  // namespace application
