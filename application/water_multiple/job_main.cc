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
 * This file contains the "main" job that Nimbus launches after loading an
 * application. All subsequent jobs are spawned from here.
 *
 * Author: Chinmayee Shah <chinmayee.shah@stanford.edu>
 */

#include "application/water_multiple/app_utils.h"
#include "application/water_multiple/data_def.h"
#include "application_utils/data_definer.h"
#include "application/water_multiple/data_names.h"
#include "application/water_multiple/parameters.h"
#include "application/water_multiple/job_main.h"
#include "application/water_multiple/job_names.h"
#include "application/water_multiple/reg_def.h"
#include "data/scratch_data_helper.h"
#include "shared/dbg.h"
#include "shared/nimbus.h"
#include "worker/job_query.h"
#include <vector>

namespace application {

JobMain::JobMain(nimbus::Application *app) {
  set_application(app);
};

nimbus::Job* JobMain::Clone() {
  return new JobMain(application());
}

void JobMain::Execute(nimbus::Parameter params, const nimbus::DataArray& da) {
  nimbus::JobQuery job_query(this);
  dbg(APP_LOG, "Executing main job\n");

  // Old code, using the code generated by python scripts -omidm
  // DefineNimbusData(this);

  nimbus::DataDefiner df(this);

  df.DefineData(APP_FACE_VEL,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_FACE_VEL_GHOST,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_PHI,
                kScale, kScale, kScale, 8, 8, 8,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_LAST_UNIQUE_PARTICLE_ID,
                kScale, kScale, kScale, 0, 0, 0,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_DT,
                kScale, kScale, kScale, 0, 0, 0,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_POS_PARTICLES,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineScratchData(APP_POS_PARTICLES,
                       kScale, kScale, kScale, 3, 3, 3,
                       kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                       true);  // Include global boundary.

  df.DefineData(APP_NEG_PARTICLES,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineScratchData(APP_NEG_PARTICLES,
                       kScale, kScale, kScale, 3, 3, 3,
                       kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                       true);  // Include global boundary.

  df.DefineData(APP_POS_REM_PARTICLES,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineScratchData(APP_POS_REM_PARTICLES,
                       kScale, kScale, kScale, 3, 3, 3,
                       kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                       true);  // Include global boundary.

  df.DefineData(APP_NEG_REM_PARTICLES,
                kScale, kScale, kScale, 3, 3, 3,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineScratchData(APP_NEG_REM_PARTICLES,
                       kScale, kScale, kScale, 3, 3, 3,
                       kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                       true);  // Include global boundary.

  // # Group I.
  df.DefineData(APP_PSI_D,
                kScale, kScale, kScale, 1, 1, 1,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_PSI_N,
                kScale, kScale, kScale, 1, 1, 1,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_PRESSURE,
                kScale, kScale, kScale, 1, 1, 1,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_FILLED_REGION_COLORS,
                kScale, kScale, kScale, 1, 1, 1,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_DIVERGENCE,
                kScale, kScale, kScale, 1, 1, 1,
                kAppPartNumX, kAppPartNumY, kAppPartNumZ,
                true);  // Include global boundary.

  // # The following data partitions should be changed if you want to run projection in different granularity.
  // # Group II.
  df.DefineData(APP_MATRIX_A,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_VECTOR_B,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_INDEX_C2M,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_INDEX_M2C,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_LOCAL_N,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_INTERIOR_N,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  // # Group III.
  df.DefineData(APP_PROJECTION_LOCAL_TOLERANCE,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_GLOBAL_TOLERANCE,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_GLOBAL_N,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_DESIRED_ITERATIONS,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  // # Group IV.
  df.DefineData(APP_PROJECTION_LOCAL_RESIDUAL,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_LOCAL_RHO,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_GLOBAL_RHO,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_GLOBAL_RHO_OLD,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_LOCAL_DOT_PRODUCT_FOR_ALPHA,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_ALPHA,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_PROJECTION_BETA,
                kScale, kScale, kScale, 0, 0, 0, 1, 1, 1,
                false);  // No global boundary.

  df.DefineData(APP_MATRIX_C,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_VECTOR_PRESSURE,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_VECTOR_Z,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.

  df.DefineData(APP_VECTOR_P_META_FORMAT,
                kScale, kScale, kScale, 1, 1, 1,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                true);  // Include global boundary.

  df.DefineData(APP_VECTOR_TEMP,
                kScale, kScale, kScale, 0, 0, 0,
                kProjAppPartNumX, kProjAppPartNumY, kProjAppPartNumZ,
                false);  // No global boundary.




  // Job setup
  int init_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> init_job_ids;
  GetNewJobID(&init_job_ids, init_job_num);

  int make_signed_distance_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> make_signed_distance_job_ids;
  GetNewJobID(&make_signed_distance_job_ids, make_signed_distance_job_num);

  int extrapolate_phi_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> extrapolate_phi_job_ids;
  GetNewJobID(&extrapolate_phi_job_ids, extrapolate_phi_job_num);

  int extrapolate_phi_2_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> extrapolate_phi_2_job_ids;
  GetNewJobID(&extrapolate_phi_2_job_ids, extrapolate_phi_2_job_num);

  int extrapolate_v_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> extrapolate_v_job_ids;
  GetNewJobID(&extrapolate_v_job_ids, extrapolate_v_job_num);

  int reseed_particles_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> reseed_particles_job_ids;
  GetNewJobID(&reseed_particles_job_ids, reseed_particles_job_num);

  int write_output_job_num = kAppPartNum;
  std::vector<nimbus::job_id_t> write_output_job_ids;
  GetNewJobID(&write_output_job_ids, write_output_job_num);

  int loop_frame_job_num = 1;
  std::vector<nimbus::job_id_t> loop_frame_job_ids;
  GetNewJobID(&loop_frame_job_ids, loop_frame_job_num);


  nimbus::IDSet<nimbus::logical_data_id_t> read, write;

  int frame = 0;
  T time = 0;
  T dt = 0;

  /*
   * Spawning initialize stage over multiple workers
   */
  for (int i = 0; i < init_job_num; ++i) {
    read.clear();
    write.clear();
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W3CentralWGB"][i], APP_FACE_VEL, APP_FACE_VEL_GHOST, APP_PHI, NULL);
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W3CentralWGB"][i], APP_POS_PARTICLES,
                        APP_NEG_PARTICLES, APP_POS_REM_PARTICLES, APP_NEG_REM_PARTICLES,
                        APP_LAST_UNIQUE_PARTICLE_ID , NULL);
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W1CentralWGB"][i],
                        APP_PRESSURE,APP_PSI_D, APP_PSI_N,  NULL);

    nimbus::Parameter init_params;
    std::string init_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &init_str);
    init_params.set_ser_data(SerializedData(init_str));
    job_query.StageJob(INITIALIZE,
                       init_job_ids[i],
                       read, write,
                       init_params, true,
                       ph.map()["kRegY2W3Central"][i]);
    job_query.Hint(init_job_ids[i], ph.map()["kRegY2W3Central"][i]);
  }
  job_query.CommitStagedJobs();

  /*
   * Spawning extrapolate phi stage over multiple workers.
   */
  for (int i = 0; i < extrapolate_phi_job_num; ++i) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_PHI,
                        APP_FACE_VEL, NULL);

    write.clear();
    LoadLogicalIdsInSet(this, &write,
                        ph.map()["kRegY2W3CentralWGB"][i], APP_PHI, NULL);

    nimbus::Parameter phi_params;
    std::string phi_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &phi_str);
    phi_params.set_ser_data(SerializedData(phi_str));

    job_query.StageJob(EXTRAPOLATE_PHI,
                       extrapolate_phi_job_ids[i],
                       read, write,
                       phi_params, true,
                       ph.map()["kRegY2W3Central"][i]);
    job_query.Hint(extrapolate_phi_job_ids[i], ph.map()["kRegY2W3Central"][i]);
  }
  job_query.CommitStagedJobs();


  /*
   * Spawning make signed distance.
   */
  for (int i = 0; i < make_signed_distance_job_num; ++i) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_PHI, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_FACE_VEL_GHOST,
                        APP_FACE_VEL, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W1Outer"][i], APP_PSI_D, APP_PSI_N, NULL);

    write.clear();
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W3CentralWGB"][i], APP_PHI, NULL);
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W1CentralWGB"][i], APP_PSI_D, APP_PSI_N,  NULL);

    std::string make_signed_distance_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &make_signed_distance_str);
    nimbus::Parameter make_signed_distance_params;
    make_signed_distance_params.set_ser_data(SerializedData(make_signed_distance_str));

    job_query.StageJob(MAKE_SIGNED_DISTANCE,
                       make_signed_distance_job_ids[i],
                       read, write,
                       make_signed_distance_params, true,
                       ph.map()["kRegY2W3Central"][i]);
    job_query.Hint(make_signed_distance_job_ids[i], ph.map()["kRegY2W3Central"][i]);
  }
  job_query.CommitStagedJobs();



  for (int i = 0; i < reseed_particles_job_num; ++i) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_FACE_VEL,
                        APP_PHI, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W1Outer"][i], APP_PSI_D,
                        APP_PSI_N, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_POS_PARTICLES,
                        APP_NEG_PARTICLES, APP_POS_REM_PARTICLES,
                        APP_NEG_REM_PARTICLES, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3CentralWGB"][i], APP_LAST_UNIQUE_PARTICLE_ID , NULL);
    write.clear();
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W3CentralWGB"][i], APP_POS_PARTICLES,
                        APP_NEG_PARTICLES, APP_POS_REM_PARTICLES,
                        APP_NEG_REM_PARTICLES, APP_LAST_UNIQUE_PARTICLE_ID,
                        NULL);

    nimbus::Parameter temp_params;
    std::string temp_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &temp_str);
    temp_params.set_ser_data(SerializedData(temp_str));
    job_query.StageJob(RESEED_PARTICLES,
                       reseed_particles_job_ids[i],
                       read, write,
                       temp_params, true,
                       ph.map()["kRegY2W3Central"][i]);
    job_query.Hint(reseed_particles_job_ids[i], ph.map()["kRegY2W3Central"][i]);
  }
  job_query.CommitStagedJobs();

  /*
   * Spawning extrapolate phi stage over multiple workers.
   */
  for (int i = 0; i < extrapolate_phi_2_job_num; ++i) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_PHI,
                        APP_FACE_VEL, NULL);

    write.clear();
    LoadLogicalIdsInSet(this, &write,
                        ph.map()["kRegY2W3CentralWGB"][i], APP_PHI, NULL);

    nimbus::Parameter phi_params;
    std::string phi_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &phi_str);
    phi_params.set_ser_data(SerializedData(phi_str));

    job_query.StageJob(EXTRAPOLATE_PHI,
                       extrapolate_phi_2_job_ids[i],
                       read, write,
                       phi_params, true,
                       ph.map()["kRegY2W3Central"][i]);
    job_query.Hint(extrapolate_phi_2_job_ids[i], ph.map()["kRegY2W3Central"][i]);
  }
  job_query.CommitStagedJobs();


  /*
   * Spawning extrapolate v stage over multiple workers
   */
  /*
  for (int i = 0; i < extrapolate_v_job_num; ++i) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i],
                        APP_FACE_VEL, APP_PHI, NULL);

    write.clear();
    LoadLogicalIdsInSet(this, &write, ph.map()["kRegY2W3Central"][i],
                        APP_FACE_VEL, NULL);

    nimbus::Parameter v_params;
    std::string v_str;
    SerializeParameter(frame, time, dt, kPNAInt,
                       kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                       kPNAInt, &v_str);
    v_params.set_ser_data(SerializedData(v_str));

    job_query.StageJob(EXTRAPOLATION,
                       extrapolate_v_job_ids[i],
                       read, write,
                       v_params, true);
  }
  job_query.CommitStagedJobs();
  */


  if (kUseGlobalWrite) {
    read.clear();
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegW3Outer"][0], APP_FACE_VEL,
                        APP_PHI, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegW1Outer"][0], APP_PSI_D,
                        APP_PSI_N, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegW3Outer"][0], APP_POS_PARTICLES,
                        APP_NEG_PARTICLES, APP_POS_REM_PARTICLES,
                        APP_NEG_REM_PARTICLES, NULL);
    LoadLogicalIdsInSet(this, &read, ph.map()["kRegW3Central"][0], APP_LAST_UNIQUE_PARTICLE_ID , NULL);
    write.clear();

    nimbus::Parameter temp_params;
    std::string temp_str;
    SerializeParameter(frame - 1, time + dt, 0, -1,
                       kDefaultRegion, kDefaultRegion,
                       kPNAInt, &temp_str);
    temp_params.set_ser_data(SerializedData(temp_str));
    job_query.StageJob(WRITE_OUTPUT,
                       write_output_job_ids[0],
                       read, write,
                       temp_params, true,
                       ph.map()["kRegW3Central"][0]);
    job_query.Hint(write_output_job_ids[0], ph.map()["kRegW3Central"][0], true);
    job_query.CommitStagedJobs();
  } else {
    for (int i = 0; i < write_output_job_num; ++i) {
      read.clear();
      LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_FACE_VEL,
                          APP_PHI, NULL);
      LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W1Outer"][i], APP_PSI_D,
                          APP_PSI_N, NULL);
      LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3Outer"][i], APP_POS_PARTICLES,
                          APP_NEG_PARTICLES, APP_POS_REM_PARTICLES,
                          APP_NEG_REM_PARTICLES, NULL);
      LoadLogicalIdsInSet(this, &read, ph.map()["kRegY2W3CentralWGB"][i], APP_LAST_UNIQUE_PARTICLE_ID , NULL);
      write.clear();

      nimbus::Parameter temp_params;
      std::string temp_str;
      SerializeParameter(frame - 1, time + dt, 0, i+1,
                         kDefaultRegion, ph.map()["kRegY2W3Central"][i],
                         kPNAInt, &temp_str);
      temp_params.set_ser_data(SerializedData(temp_str));
      job_query.StageJob(WRITE_OUTPUT,
                         write_output_job_ids[i],
                         read, write,
                         temp_params, true,
                         ph.map()["kRegY2W3Central"][i]);
      job_query.Hint(write_output_job_ids[i], ph.map()["kRegY2W3Central"][i]);
    }
    job_query.CommitStagedJobs();
  }

  /*
   * Spawning loop frame job.
   */
  read.clear();
  write.clear();

  nimbus::Parameter loop_params;
  std::string loop_str;
  SerializeParameter(frame, kPNAFloat, kPNAFloat, kPNAInt,
                     kDefaultRegion,
                     kPNAReg,
                     kPNAInt, &loop_str);
  loop_params.set_ser_data(SerializedData(loop_str));

  job_query.StageJob(LOOP_FRAME,
                     loop_frame_job_ids[0],
                     read, write,
                     loop_params, false,
                     ph.map()["kRegW3Central"][0],
                     true);
  job_query.Hint(loop_frame_job_ids[0], ph.map()["kRegW3Central"][0], true);
  job_query.CommitStagedJobs();

  dbg(APP_LOG, "Completed executing main job\n");

  dbg(APP_LOG, "Print job dependency figure.\n");
  job_query.GenerateDotFigure("job_main.dot");
}

} // namespace application
