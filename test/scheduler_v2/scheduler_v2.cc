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
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#include <math.h>
#include "./scheduler_v2.h"

SchedulerV2::SchedulerV2(unsigned int p)
: Scheduler(p) {
  initialized_domains_ = false;
}

void SplitDimensions(size_t worker_num, float *num_x, float *num_y, float *num_z);

bool SchedulerV2::GetWorkerToAssignJob(JobEntry* job, SchedulerWorker*& worker) {
  size_t worker_num = server_->worker_num();
  GeometricRegion global_bounding_region =
    data_manager_->global_bounding_region();

  if ((!initialized_domains_) ||
      (worker_num_ != worker_num) ||
      (global_bounding_region_ != global_bounding_region)) {
    global_bounding_region_ = global_bounding_region;
    worker_num_ = worker_num;
    worker_domains_.clear();

    float num_x, num_y, num_z;
    SplitDimensions(worker_num, &num_x, &num_y, &num_z);

    int_dimension_t dx =
      static_cast<int_dimension_t>(global_bounding_region_.dx() / num_x);
    int_dimension_t dy =
      static_cast<int_dimension_t>(global_bounding_region_.dy() / num_y);
    int_dimension_t dz =
      static_cast<int_dimension_t>(global_bounding_region_.dz() / num_z);


    for (size_t i = 0; i < num_x; ++i) {
      for (size_t j = 0; j < num_y; ++j) {
        for (size_t k = 0; k < num_z; ++k) {
          worker_domains_.push_back(GeometricRegion(
                global_bounding_region_.x() + i * dx,
                global_bounding_region_.y() + j * dy,
                global_bounding_region_.z() + k * dz,
                dx,
                dy,
                dz));
        }
      }
    }
    initialized_domains_ = true;
  }

  std::vector<int> workers_rank(worker_num, 0);

  IDSet<logical_data_id_t> union_set = job->union_set();
  IDSet<logical_data_id_t>::IDSetIter iter;
  for (iter = union_set.begin(); iter != union_set.end(); ++iter) {
    const LogicalDataObject* ldo;
    ldo = data_manager_->FindLogicalObject(*iter);
    for (size_t i = 0; i < worker_num; ++i) {
      if (worker_domains_[i].Intersects(ldo->region())) {
        ++workers_rank[i];
      }
    }
  }

  // find the worker that wins the poll.
  worker_id_t w_id = 1;
  int count = workers_rank[0];
  for (size_t i = 1; i < worker_num; ++i) {
    if (count < workers_rank[i]) {
      count = workers_rank[i];
      w_id = i + 1;
    }
  }

  std::cout << "Picked worker: " << w_id << " for job: " << job->job_name() << std::endl;
  return server_->GetSchedulerWorkerById(worker, w_id);
}


void SplitDimensions(size_t worker_num, float *num_x, float *num_y, float *num_z) {
  switch (worker_num) {
    case 1 :
      *num_x = 1;
      *num_y = 1;
      *num_z = 1;
      break;
    case 2 :
      *num_x = 1;
      *num_y = 2;
      *num_z = 1;
      break;
    case 3 :
      *num_x = 1;
      *num_y = 3;
      *num_z = 1;
      break;
    case 4 :
      *num_x = 2;
      *num_y = 2;
      *num_z = 1;
      break;
    case 5 :
      *num_x = 1;
      *num_y = 5;
      *num_z = 1;
      break;
    case 6 :
      *num_x = 2;
      *num_y = 3;
      *num_z = 1;
      break;
    case 7 :
      *num_x = 1;
      *num_y = 7;
      *num_z = 1;
      break;
    case 8 :
      *num_x = 1;
      *num_y = 1;
      *num_z = 1;
      break;
    default:
      dbg(DBG_ERROR, "ERROR: Do not know how to split!");
      exit(-1);
  }
}
