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
 * Author: Chinmayee Shah <chinmayee.shah@stanford.edu>
 */

#include "advection.h"
#include "app_config.h"
#include "app_utils.h"
#include "assert.h"
#include "data_face_arrays.h"
#include "data_utils.h"
#include "physbam_include.h"
#include "proto_files/params.pb.h"
#include "shared/geometric_region.h"
#include "shared/nimbus.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <vector>
#include "water_app.h"
#include "water_data_driver.h"
#include "water_driver.h"

using nimbus::Data;
using nimbus::Job;
using nimbus::Application;

/* Assumption: types_list contains a maximum of one kWaterDriver and one
 * kFaceArray. */
#define GetJobData()                                                        \
    WaterApp *water_app = (WaterApp *)application();                        \
    WaterDriver<TV> *driver = NULL;                                         \
    NonAdvData<TV, T> *sim_data = NULL;                                     \
    FaceArrayList fvleft;                                                   \
    FaceArrayList fvright;                                                  \
    for (unsigned int i = 0; i < da.size(); i++) {                          \
        switch (da[i]->get_debug_info()) {                                  \
            case driver_id:                                                 \
                driver = (WaterDriver<TV> *)da[i];                          \
                break;                                                      \
            case non_adv_id:                                                \
                sim_data = (NonAdvData<TV, T> *)da[i];                      \
                break;                                                      \
            case face_array_id:                                             \
                FaceArray *temp = (FaceArray *)da[i];                       \
                if (temp->left_or_right == 0)                               \
                    fvleft.push_back(temp);                                 \
                else                                                        \
                    fvright.push_back(temp);                                \
                break;                                                      \
        }                                                                   \
    }                                                                       \
    if (!water_app)                                                         \
        asm volatile("" : : : "memory");
    // asm is just a barrier to allow code to compile with unused variables    

namespace {
    /* Initialize/ declare application constants. */

    typedef ::PhysBAM::GRID<TV> T_GRID;
    typedef ::PhysBAM::RANGE<TV> T_RANGE;

    TV_INT main_size(kMainSize, kMainSize);

    const int knx[] = {
        1,
        kMainSize/2-kGhostSize+1,
        kMainSize/2+1,
        kMainSize/2+kGhostSize+1};
    const int kny[] = {1};
    const int kndxl[] = {
        kMainSize/2-kGhostSize,
        kGhostSize,
        kGhostSize};
    const int kndxr[] = {
        kGhostSize,
        kGhostSize,
        kMainSize/2-kGhostSize};
    const int kndy[] = {kMainSize};
    nimbus::GeometricRegion kwhole_region(
            1, 1, 0,
            kMainSize, kMainSize, 0);
    nimbus::GeometricRegion kleft_region(
            1, 1, 0,
            kMainSize/2, kMainSize, 0);
    nimbus::GeometricRegion kright_region(
            kMainSize/2+1, 1, 0,
            kMainSize/2, kMainSize, 0);
    nimbus::GeometricRegion kleft_ghost_region(
            -kGhostSize+1, -kGhostSize+1, 0,
            kMainSize/2+2*kGhostSize, kMainSize/2+2*kGhostSize, 0);
    nimbus::GeometricRegion kright_ghost_region(
            kMainSize/2-kGhostSize+1, -kGhostSize+1, 0,
            kMainSize/2+2*kGhostSize, kMainSize/2+2*kGhostSize, 0);
    ::std::vector < ::nimbus::GeometricRegion > kleft_regions;
    ::std::vector < ::nimbus::GeometricRegion > kright_regions;
    ::std::vector < ::std::string > kleft_adv_types;
    ::std::vector < ::std::string > kright_adv_types;
    const int pieces = 3;  // pieces of velocity per worker
    const int workers = 2; // number of workers
} // namespace application

WaterApp::WaterApp() {
};

void WaterApp::Load() {
    printf("Worker beginning to load application\n");

    /* Initialize application constants. */
    for (int i = 0; i < pieces; i++) {
        for (int j = 0; j < 1; j++) {
            kleft_regions.push_back(
                    GeometricRegion(
                        knx[i], kny[j], 0,
                        kndxl[i], kndy[j], 0));
            kright_regions.push_back(
                    GeometricRegion(
                        knx[i+1], kny[j], 0,
                        kndxr[i], kndy[j], 0));
        }
    }
    for (int i = 0; i < pieces; i++) {
        std::string ssl;
        char bl[2048];
        snprintf(bl, sizeof(bl), "advection_left_%i",i);
        ssl+=bl;
        std::string ssr;
        char br[2048];
        snprintf(br, sizeof(br), "advection_right_%i",i);
        ssr+=br;
        kleft_adv_types.push_back(ssl);
        kright_adv_types.push_back(ssr);
    }

    LOG::Initialize_Logging(false, false, 1<<30, true, 1);

    /* Declare data types. */
    RegisterData("water_driver", new WaterDriver<TV>( STREAM_TYPE(T()) ) );
    RegisterData("sim_data", new NonAdvData<TV, T>(kMainSize));
    /* Declare velocity types. */
    for (int i = 0; i < pieces; i++) {
        printf("Registering data with region %s\n", kleft_regions[i].toString().c_str());
        printf("Registering data with region %s\n", kright_regions[i].toString().c_str());
        RegisterData(
                kleft_adv_types[i],
                new FaceArray(kleft_regions[i], 0));
        RegisterData(
                kright_adv_types[i],
                new FaceArray(kright_regions[i], 1));
    }

    /* Declare job types. */
    RegisterJob("main", new Main(this));
    RegisterJob("init", new Init(this));
    RegisterJob("loop", new Loop(this));
    RegisterJob("uptoadvect", new UptoAdvect(this));
    RegisterJob("advect", new Advect(this));
    RegisterJob("afteradvect", new AfterAdvect(this));
    RegisterJob("writeframe", new WriteFrame(this));

    printf("Finished creating job and data definitions\n");

    /* Application data initialization -- initialization of partition specific
     * data happens later. */
    set_advection_scalar(new ::PhysBAM::ADVECTION_SEMI_LAGRANGIAN_UNIFORM
            < ::PhysBAM::GRID<TV>,T>());

    set_boundary(new ::PhysBAM::BOUNDARY_UNIFORM< ::PhysBAM::GRID<TV>, T>());
    ::PhysBAM::VECTOR< ::PhysBAM::VECTOR<bool, 2>, TV::dimension>
        domain_boundary;
    for(int i=1;i<=TV::dimension;i++) {
        domain_boundary(i)(1)=true;
        domain_boundary(i)(2)=true;
    }
    domain_boundary(2)(2)=false;
    ::PhysBAM::VECTOR< ::PhysBAM::VECTOR<bool,2>,TV::dimension>
        domain_open_boundaries = ::PhysBAM::VECTOR_UTILITIES
        ::Complement(domain_boundary);
    boundary()->Set_Constant_Extrapolation(domain_open_boundaries);

    printf("Finished loading application\n");
}

Main::Main(Application *app) {
    set_application(app);
};

Job* Main::Clone() {
    printf("Cloning main job\n");
    return new Main(application());
};

void Main::Execute(Parameter params, const DataArray& da) {
    printf("Begin main\n");

    /* Define data as necessary for jobs here. */
    Parameter par_data;
    IDSet<partition_id_t> neighbor_partitions;
    partition_id_t partition_id = 0;
    std::vector<data_id_t> d;
    GetNewDataID(&d, pieces*2+2);
    // water driver: non adv data
    DefineData(
            "water_driver",
            d[0],
            partition_id,
            neighbor_partitions,
            par_data);
    // sim data: non adv data
    DefineData("sim_data", d[1], partition_id, neighbor_partitions, par_data);
    // adv data
    for (int i = 0; i < pieces; i++) {
        DefineData(
                kleft_adv_types[i],
                d[2+2*i],
                partition_id,
                neighbor_partitions,
                par_data
                );
        DefineData(
                kright_adv_types[i],
                d[3+2*i],
                partition_id+1,
                neighbor_partitions,
                par_data
                );
    }

    /* Spawn required jobs -- we have one init job and one loop job here.
     * Order:
     * - common init
     * - loop
     */
    int job_num = 2;
    std::vector<job_id_t> j;
    GetNewJobID(&j, job_num);
    IDSet<data_id_t> read, write;
    IDSet<job_id_t> before, after;
    Parameter par_job;
    IDSet<data_id_t> alldata;
    for (unsigned int i = 0; i < d.size(); i++) {
        alldata.insert(d[i]);
        read.insert(d[i]);
        write.insert(d[i]);
    }
    // init
    before.clear(); after.clear();
    after.insert(j[1]);
    SpawnComputeJob("init", j[0], read, write, before, after, par_job);
    printf("Spawned init\n");
    before.clear(); after.clear();
    before.insert(j[0]);
    // TODO: since compute dt is not a separate job right now, insering all
    // data into for loop job. this should be fixed later on -- need to
    // restructure the decomposition.
    par_job.set_idset(alldata);
    SpawnComputeJob("loop", j[1], read, write, before, after, par_job);
    printf("Spawned loop\n");
    printf("Completed main\n");
};

Init::Init(Application *app) {
    set_application(app);
}

Job* Init::Clone() {
    printf("Cloning init job\n");
    return new Init(application());
};

void Init::Execute(Parameter params, const DataArray& da) {
    printf("Executing init job\n");
    GetJobData();
    T_GRID grid(main_size, T_RANGE::Unit_Box(), true);
    T_FACE_ARRAY *fv = new  T_FACE_ARRAY(grid);
    driver->face_velocities = fv;
    driver->sim_data = sim_data;
    Add_Source(sim_data);
    sim_data->incompressible->
        Set_Custom_Boundary(*water_app->boundary());
    int frame = 0;
    sim_data->initialize(
            driver,
            fv,
            frame);
    driver->Write_Output_Files(driver->current_frame);
    FaceArray::Update_Regions(
            fv,
            fvleft,
            kleft_region,
            0,
            0);
    FaceArray::Update_Regions(
            fv,
            fvright,
            kright_region,
            0,
            0);
    sim_data->incompressible->
        Set_Custom_Advection(*(water_app->advection_scalar()));
    sim_data->particle_levelset_evolution->Levelset_Advection(1).
        Set_Custom_Advection(*(water_app->advection_scalar()));
    // TODO: this is just for testing serialization/ deserialization, remove it later on
//    for (unsigned int i = 0; i < fvleft.size(); i++) {
//        bool checker;
//        SerializedData ser_data;
//        ser_data.set_size(1);
//        checker = fvleft[i]->Serialize(&ser_data);
//        assert(checker);
//        fvleft[i]->Destroy();
//        fvleft[i]->Create();
//        Data *temp = fvleft[i];
//        checker = (fvleft[i])->DeSerialize(ser_data, &temp);
//        assert(checker);
//    }
//    // TODO: this is just for testing glue, remove it later on
//    delete(fv);
//    fv = new  T_FACE_ARRAY(grid);
//    driver->face_velocities = fv;
//    FaceArray::Glue_Regions(
//            fv,
//            fvleft,
//            kleft_region,
//            0,
//            0);
//    FaceArray::Glue_Regions(
//            fv,
//            fvright,
//            kright_region,
//            0,
//            0);
//
    printf("*** Rewriting ....\n");
    driver->Write_Output_Files(driver->current_frame);
    delete(fv);
    printf("Successfully completed init job\n");
};

UptoAdvect::UptoAdvect(Application *app) {
  set_application(app);
};

Job* UptoAdvect::Clone() {
    printf("Cloning upto advect job\n");
    return new UptoAdvect(application());
};

void UptoAdvect::Execute(Parameter params, const DataArray& da) {
//    printf("@@ Running upto advect\n");
//    std::vector<int> types_list;
//    types_list.push_back(driver_id);
//    types_list.push_back(non_adv_id);
//    for (int i = 0; i < 9; i++) {
//        types_list.push_back(face_array_id);
//    }
//    GetJobData();
//    job_data.sim_data->BeforeAdvection(
//            job_data.driver,
//            job_data.central_vels[0]->data());
};

Advect::Advect(Application *app) {
    set_application(app);
};

Job* Advect::Clone() {
    printf("Cloning advect job\n");
    return new Advect(application());
};

void Advect::Execute(Parameter params, const DataArray& da) {
//    printf("@@ Running advect\n");
//    std::vector<int> types_list;
//    types_list.push_back(driver_id);
//    for (int i = 0; i < 9; i++) {
//        types_list.push_back(face_array_id);
//    }
//    GetJobData();
//    ::parameters::AdvVelPar adv_vel_par_pb;
//    std::string str(params.ser_data().data_ptr_raw(),
//        params.ser_data().size());
//    adv_vel_par_pb.ParseFromString(str);
//    T_FACE_ARRAY *face_vel_extended = 
//        new T_FACE_ARRAY(*(job_data.central_vels[0]->grid()), kGhostSize);
//    job_data.central_vels[0]->Extend_Array(
//            job_data.central_vels[0]->data(),
//            face_vel_extended,
//            water_app->boundary(),
//            kGhostSize,
//            job_data.driver->dt + job_data.driver->time,
//            true);
//    Advect_Velocities(job_data.central_vels[0], face_vel_extended, water_app,
//            adv_vel_par_pb.dt(), adv_vel_par_pb.time());
//    delete(face_vel_extended);
};

AfterAdvect::AfterAdvect(Application *app) {
    set_application(app);
};

Job* AfterAdvect::Clone() {
    printf("Cloning after advect job\n");
    return new AfterAdvect(application());
};

void AfterAdvect::Execute(Parameter params, const DataArray& da) {
//    printf("@@ Running after advect\n");
//    std::vector<int> types_list;
//    types_list.push_back(driver_id);
//    types_list.push_back(non_adv_id);
//    for (int i = 0; i < 9; i++) {
//        types_list.push_back(face_array_id);
//    }
//    GetJobData();
//    job_data.sim_data->AfterAdvection(
//            job_data.driver,
//            job_data.central_vels[0]->data());
};

Loop::Loop(Application *app) {
    set_application(app);
};

Job* Loop::Clone() {
    printf("Cloning loop job\n");
    return new Loop(application());
};

void Loop::Execute(Parameter params, const DataArray& da) {
    printf("@@Executing forloop job\n");
    GetJobData();
    T_GRID grid(main_size, T_RANGE::Unit_Box(), true);
    T_FACE_ARRAY *fv = new  T_FACE_ARRAY(grid);
    FaceArray::Glue_Regions(
            fv,
            fvleft,
            kleft_region,
            0,
            0);
    FaceArray::Glue_Regions(
            fv,
            fvright,
            kright_region,
            0,
            0);
    driver->face_velocities = fv;
    driver->sim_data = sim_data;
    driver->IncreaseTime();
    if (!driver->CheckProceed()) {
        printf("... Simulation completed ...\n");
    }
    else {
        printf("Spawning new simulation jobs ...\n");

        Parameter par;
        IDSet<job_id_t> before, after;
        IDSet<data_id_t> read, write;
        std::vector<job_id_t> j;
        GetNewJobID(&j, 5);
            
//        par.set_ser_data(SerializedData(""));
//        before.clear(); after.clear();
//        read.clear(); write.clear();
//        after.insert(j[1]);
//        read.insert(d[0]); read.insert(d[1]);
//        read.insert(d[2]); read.insert(d[3]);
//        write.insert(d[0]); write.insert(d[1]);
//        write.insert(d[2]); write.insert(d[3]);
//        SpawnComputeJob("uptoadvect", j[0], read, write, before, after, par);
//        printf("Spawned upto advect\n");
//        ::parameters::AdvVelPar adv_vel_par_pb;
//        adv_vel_par_pb.set_dt(job_data.driver->dt);
//        adv_vel_par_pb.set_time(job_data.driver->time);
//        std::string str;
//        adv_vel_par_pb.SerializeToString(&str);
//        par.set_ser_data(SerializedData(str));
//        before.clear(); after.clear();
//        read.clear(); write.clear();
//        before.insert(j[0]);
//        after.insert(j[2]);
//        read.insert(d[0]);
//        read.insert(d[2]); read.insert(d[3]);
//        write.insert(d[0]);
//        write.insert(d[2]); write.insert(d[3]);
//        SpawnComputeJob("advect", j[1], read, write, before, after, par);
//        printf("Spawned advect\n");
//        par.set_ser_data(SerializedData(""));
//        before.clear(); after.clear();
//        read.clear(); write.clear();
//        before.insert(j[1]);
//        after.insert(j[3]);
//        read.insert(d[0]); read.insert(d[1]);
//        read.insert(d[2]); read.insert(d[3]);
//        write.insert(d[0]); write.insert(d[1]);
//        write.insert(d[2]); write.insert(d[3]);
//        SpawnComputeJob("afteradvect", j[2], read, write, before, after, par);
//        printf("Spawned afteradvect\n");
        printf("Spawning writeframe\n");
        par.set_ser_data(SerializedData(""));
        before.clear(); after.clear();
        read.clear(); write.clear();
//        before.insert(j[2]);
//        after.insert(j[4]);
        for (unsigned int i = 0; i < da.size(); i++)
            read.insert(da[i]->id());
        SpawnComputeJob("writeframe", j[3], read, write, before, after, par);
        printf("Spawned writeframe\n");
//        par.set_ser_data(SerializedData(""));
//        before.clear(); after.clear();
//        read.clear(); write.clear();
//        before.insert(j[3]);
//        read.insert(d[0]); read.insert(d[1]);
//        read.insert(d[2]); read.insert(d[3]);
//        write.insert(d[0]); write.insert(d[1]);
//        write.insert(d[2]); write.insert(d[3]);
//        SpawnComputeJob("loop", j[4], read, write, before, after, par);
//        printf("Spawned loop\n");
    }
    delete(fv);
    printf("Exiting loop job\n");
};

WriteFrame::WriteFrame(Application *app) {
    set_application(app);
};

Job* WriteFrame::Clone() {
    printf("Cloning write frame job\n");
    return new WriteFrame(application());
};

void WriteFrame::Execute(Parameter params, const DataArray& da) {
    printf( "@@ Executing write frame job\n");
    GetJobData();
    T_GRID grid(main_size, T_RANGE::Unit_Box(), true);
    T_FACE_ARRAY *fv = new  T_FACE_ARRAY(grid);
//    FaceArray::Glue_Regions(
//            fv,
//            fvleft,
//            kleft_region,
//            0,
//            0);
//    FaceArray::Glue_Regions(
//            fv,
//            fvright,
//            kright_region,
//            kMainSize/2,
//            0);
//    driver->face_velocities = fv;
//    driver->sim_data = sim_data;
//    if (driver->IsFrameDone()) {
//        driver->Write_Output_Files(driver->current_frame);
//    }
    delete(fv);
}
