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
  * Nimbus abstraction of an application.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#include <time.h>
#include "worker/application.h"
#include "worker/cache_manager.h"

using namespace nimbus; // NOLINT

Application::Application() {
  pthread_mutex_init(&lock_job_table_, NULL);
  pthread_mutex_init(&lock_data_table_, NULL);
}

Application::~Application() {
  delete cache_manager_;
  pthread_mutex_destroy(&lock_job_table_);
  pthread_mutex_destroy(&lock_data_table_);
}

void Application::Load() {
  std::cout << "Loaded Nimbus base application." << std::endl;
}

void Application::Start(SchedulerClient* client,
    IDMaker* id_maker,
    WorkerLdoMap* ldo_map) {
  std::cout << "Running Nimbus application: " << id_ << std::endl;
  client_ = client;
  id_maker_ = id_maker;
  ldo_map_ = ldo_map;
  cache_manager_ = new CacheManager();
  Load();
}

// Thread-safe.
void Application::RegisterJob(std::string name, Job* j) {
  LockGuard lock(&lock_job_table_);
  job_table_[name] = j;
}

// Thread-safe.
void Application::RegisterData(std::string name, Data* d) {
  LockGuard lock(&lock_data_table_);
  data_table_[name] = d;
}

// Thread-safe.
void Application::SpawnComputeJob(const std::string& name,
                                  const job_id_t& id,
                                  const IDSet<logical_data_id_t>& read,
                                  const IDSet<logical_data_id_t>& write,
                                  const IDSet<job_id_t>& before,
                                  const IDSet<job_id_t>& after,
                                  const job_id_t& parent_id,
                                  const job_id_t& future_id,
                                  const bool& sterile,
                                  const GeometricRegion& region,
                                  const Parameter& params) {
  // static double construct_time = 0;
  // struct timespec start_time;
  // clock_gettime(CLOCK_REALTIME, &start_time);

  SpawnComputeJobCommand cm(name,
                            ID<job_id_t>(id),
                            read,
                            write,
                            before,
                            after,
                            ID<job_id_t>(parent_id),
                            ID<job_id_t>(future_id),
                            sterile,
                            region,
                            params);

  // struct timespec t;
  // clock_gettime(CLOCK_REALTIME, &t);
  // construct_time += difftime(t.tv_sec, start_time.tv_sec)
  //     + .000000001 * (static_cast<double>(t.tv_nsec - start_time.tv_nsec));
  // printf("Construct command time %f\n", construct_time);

  client_->SendCommand(&cm);
}

// Thread-safe.
void Application::SpawnCopyJob(const job_id_t& id,
                               const logical_data_id_t& from_logical_id,
                               const logical_data_id_t& to_logical_id,
                               const IDSet<job_id_t>& before,
                               const IDSet<job_id_t>& after,
                               const job_id_t& parent_id) {
  SpawnCopyJobCommand cm(ID<job_id_t>(id), ID<logical_data_id_t>(from_logical_id),
      ID<logical_data_id_t>(to_logical_id), before, after, ID<job_id_t>(parent_id));
  client_->SendCommand(&cm);
}

// Thread-safe.
void Application::DefineData(const std::string& name,
                             const logical_data_id_t& logical_data_id,
                             const partition_id_t& partition_id,
                             const IDSet<partition_id_t>& neighbor_partitions,
                             const job_id_t& parent_id) {
  ID<logical_data_id_t> logical_id_made(logical_data_id);
  ID<partition_id_t> partition_id_made(partition_id);
  ID<job_id_t> parent_id_made(parent_id);

  ldo_map_->AddLogicalObject(logical_data_id, name, partition_id);
  DefineDataCommand cm(name, logical_id_made, partition_id_made,
                       neighbor_partitions, parent_id_made);
  client_->SendCommand(&cm);
}

// Thread-safe.
void Application::DefinePartition(const ID<partition_id_t>& partition_id,
                                  const GeometricRegion& r) {
  ldo_map_->AddPartition(partition_id.elem(), r);
  DefinePartitionCommand cm(partition_id, r);
  client_->SendCommand(&cm);
}

// Thread-safe.
void Application::TerminateApplication(const exit_status_t& exit_status) {
  TerminateCommand*  cm = new TerminateCommand(ID<exit_status_t>(exit_status));
  client_->SendCommand(cm);
  delete cm;
}

// Thread-safe.
Job* Application::CloneJob(std::string name) {
  LockGuard lock(&lock_job_table_);
  if (job_table_.count(name) == 0) {
    dbg(DBG_ERROR, "ERROR: job name %s is not registered in the application.\n", name.c_str()); // NOLINT
    exit(-1);
  } else {
    return job_table_[name]->Clone();
  }
}

// Thread-safe.
Data* Application::CloneData(std::string name) {
  LockGuard lock(&lock_data_table_);
  if (data_table_.count(name) == 0) {
    dbg(DBG_ERROR, "ERROR: data name %s is not registered in the application.\n", name.c_str()); // NOLINT
    exit(-1);
  } else {
    return data_table_[name]->Clone();
  }
}

// Thread-safe.
bool Application::GetNewJobID(std::vector<job_id_t>* result, size_t req_num) {
  return id_maker_->GetNewJobID(result, req_num);
}

// Thread-safe.
bool Application::GetNewLogicalDataID(std::vector<logical_data_id_t>* result, size_t req_num) {
  return id_maker_->GetNewLogicalDataID(result, req_num);
}

// Thread-safe.
bool Application::GetPartition(partition_id_t id, GeometricRegion* r) {
  if (ldo_map_ == NULL) {
    std::cout << "Error: GetLogicalObject, ldo_map_ has not been set." << std::endl;
    exit(-1);
  } else {
    return ldo_map_->FindPartition(id, r);
  }
}

// Thread-safe.
const LogicalDataObject* Application::GetLogicalObject(logical_data_id_t id) {
  if (ldo_map_ == NULL) {
    std::cout << "Error: GetLogicalObject, ldo_map_ has not been set." << std::endl;
    exit(-1);
  } else {
    return ldo_map_->FindLogicalObject(id);
  }
}

// Thread-safe.
int Application::GetCoveredLogicalObjects(CLdoVector* result,
     const std::string& variable,
     const GeometricRegion* r) {
  if (ldo_map_ == NULL) {
    return false;
  } else {
    return ldo_map_->FindCoveredLogicalObjects(variable, r, result);
  }
}

// Thread-safe.
int Application::GetAdjacentLogicalObjects(CLdoVector* result,
    const std::string& variable,
    const GeometricRegion* r) {
  if (ldo_map_ == NULL) {
    return false;
  } else {
    return ldo_map_->FindAdjacentLogicalObjects(variable, r, result);
  }
}

// Thread-safe.
int Application::GetIntersectingLogicalObjects(CLdoVector* result,
    const std::string& variable,
    const GeometricRegion* r) {
  if (ldo_map_ == NULL) {
    return false;
  } else {
    return ldo_map_->FindIntersectingLogicalObjects(variable, r, result);
  }
}

CacheManager* Application::cache_manager() const {
  return cache_manager_;
}
