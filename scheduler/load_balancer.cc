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
  * This is the base class that serves the scheduler regarding job assignment
  * queries in the cluster. It tries to minimize the completion of simulation
  * by reducing the cost of communication by locality aware data placement and
  * mitigate the effect of stragglers in the system by adapting the job
  * assignment strategies to the dynamic changes of the cloud.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */


#include "scheduler/load_balancer.h"

namespace nimbus {

LoadBalancer::LoadBalancer() {
  updated_info_ = false;
  cluster_map_ = NULL;
  job_manager_ = NULL;
  data_manager_ = NULL;
}

LoadBalancer::LoadBalancer(ClusterMap* cluster_map)
  : cluster_map_(cluster_map) {
  updated_info_ = false;
}

LoadBalancer::~LoadBalancer() {
}

ClusterMap* LoadBalancer::cluster_map() {
  return cluster_map_;
}

void LoadBalancer::set_cluster_map(ClusterMap* cluster_map) {
  cluster_map_ = cluster_map;
}

void LoadBalancer::set_job_manager(JobManager *job_manager) {
  job_manager_ = job_manager;
}

void LoadBalancer::set_data_manager(DataManager *data_manager) {
  data_manager_ = data_manager;
}

void LoadBalancer::Run() {
  // TODO(omidm): Fill out the method.
}


bool LoadBalancer::GetWorkerToAssignJob(
    JobEntry *job, SchedulerWorker*& worker) {
  // TODO(omidm): Fill out the method.
  return false;
}


void LoadBalancer::NotifyJobAssignment(
    const JobEntry *job, const SchedulerWorker* worker) {
  double time = log_.GetTime();

  if (job->job_type() != JOB_COMP) {
    return;
  }

  JobProfile *job_profile =
    new JobProfile(
        job->job_type(),
        job->job_name(),
        job->job_id(),
        job->parent_job_id(),
        worker->worker_id(),
        job->sterile());

  job_profile->set_assign_time(time);
  job_profile->set_assigned(true);


  Vertex<JobEntry, job_id_t>* vertex;
  job_manager_->job_graph_p()->GetVertex(job->job_id(), &vertex);

  typename Edge<JobEntry, job_id_t>::Iter iter;
  for (iter = vertex->incoming_edges()->begin(); iter != vertex->incoming_edges()->end(); ++iter) {
    JobEntry *j = iter->second->start_vertex()->entry();
    if (j->done()) {
      JobHistory::iterator it = job_history_.find(j->job_id());
      if (it != job_history_.end()) {
        JobProfile *jp = it->second;
        job_profile->add_log_entry(
            jp->worker_id(), jp->job_id(), jp->done_time());
      } else {
        dbg(DBG_WARN, "WARNING: Load balancer, could not find done job in job history.");
        exit(-1);
      }
    } else {
      job_profile->waiting_set_p()->insert(j->job_id());
    }
  }

  if (job_profile->waiting_set_p()->size() == 0) {
    job_profile->set_ready_time(time);
    job_profile->set_ready(true);
  }

  job_history_[job->job_id()] = job_profile;
}

void LoadBalancer::NotifyJobDone(const JobEntry *job) {
  // TODO(omidm): Fill out the method.
}



}  // namespace nimbus
