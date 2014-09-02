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
  * This is the base job assigner module. It provides methods required for
  * binding a job to a worker and submitting the compute and copy jobs to the
  * worker.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */


#include "scheduler/job_assigner.h"
#include "scheduler/load_balancer.h"

#define LB_UPDATE_RATE 100
#define JOB_ASSIGNER_THREAD_NUM 1

namespace nimbus {

JobAssigner::JobAssigner() {
  Initialize();
}

void JobAssigner::Initialize() {
  thread_num_ = 0;
  pending_assignment_ = 0;
  server_ = NULL;
  id_maker_ = NULL;
  job_manager_ = NULL;
  data_manager_ = NULL;
  load_balancer_ = NULL;
  log_.set_file_name("job_assigner_log");
}

JobAssigner::~JobAssigner() {
}

void JobAssigner::set_id_maker(IDMaker *id_maker) {
  id_maker_ = id_maker;
}

void JobAssigner::set_server(SchedulerServer *server) {
  server_ = server;
}

void JobAssigner::set_job_manager(JobManager *job_manager) {
  job_manager_ = job_manager;
}

void JobAssigner::set_data_manager(DataManager *data_manager) {
  data_manager_ = data_manager;
}

void JobAssigner::set_load_balancer(LoadBalancer *load_balancer) {
  load_balancer_ = load_balancer;
}

void JobAssigner::set_thread_num(size_t thread_num) {
  thread_num_ = thread_num;
}

void JobAssigner::Run() {
  for (size_t i = 0; i < thread_num_; ++i) {
    job_assigner_threads_.push_back(
        new boost::thread(boost::bind(&JobAssigner::JobAssignerThread, this)));
  }
}


void JobAssigner::JobAssignerThread() {
  while (true) {
    JobEntry *job;
    {
      boost::unique_lock<boost::recursive_mutex> job_queue_lock(job_queue_mutex_);

      while (job_queue_.size() == 0) {
        job_queue_cond_.wait(job_queue_lock);
      }

      JobEntryList::iterator iter = job_queue_.begin();
      job = *iter;
      job_queue_.erase(iter);
      ++pending_assignment_;
    }

    if (!AssignJob(job)) {
      dbg(DBG_ERROR, "ERROR: JobAssigner: could not assign job %lu.\n", job->job_id());
      exit(-1);
    }

    {
      boost::unique_lock<boost::recursive_mutex> job_queue_lock(job_queue_mutex_);
      --pending_assignment_;
      job_queue_cond_.notify_all();
    }
  }
}

void JobAssigner::AssignJobs(const JobEntryList& list) {
  if (thread_num_ == 0) {
    boost::unique_lock<boost::recursive_mutex> job_queue_lock(job_queue_mutex_);
    assert(job_queue_.size() == 0);
    job_queue_ = list;
    JobEntry *job;
    JobEntryList::iterator iter = job_queue_.begin();
    for (; iter != job_queue_.end();) {
      job = *iter;
      if (!AssignJob(job)) {
        dbg(DBG_ERROR, "ERROR: JobAssigner: could not assign job %lu.\n", job->job_id());
        exit(-1);
      }
      job_queue_.erase(iter++);
    }
  } else {
    boost::unique_lock<boost::recursive_mutex> job_queue_lock(job_queue_mutex_);
    assert(job_queue_.size() == 0);
    job_queue_ = list;
    job_queue_cond_.notify_all();

    while ((job_queue_.size() > 0) || (pending_assignment_ > 0)) {
      job_queue_cond_.wait(job_queue_lock);
    }
  }
}

bool JobAssigner::AssignJob(JobEntry *job) {
  SchedulerWorker* worker = job->assigned_worker();

  // log_.log_StartTimer();
  job_manager_->ResolveJobDataVersions(job);
  // log_.log_StopTimer();
  // std::cout << "versioning: " << log_.timer() << std::endl;

  bool prepared_data = true;
  IDSet<logical_data_id_t>::ConstIter it;
  for (it = job->union_set_p()->begin(); it != job->union_set_p()->end(); ++it) {
    if (!PrepareDataForJobAtWorker(job, worker, *it)) {
      prepared_data = false;
      break;
    }
  }

  if (prepared_data) {
    job_manager_->UpdateJobBeforeSet(job);
    SendComputeJobToWorker(worker, job);

    job_manager_->NotifyJobAssignment(job);
    load_balancer_->NotifyJobAssignment(job);

    return true;
  }

  return false;
}


bool JobAssigner::PrepareDataForJobAtWorker(JobEntry* job,
                                            SchedulerWorker* worker,
                                            logical_data_id_t l_id) {
  bool reading = job->read_set_p()->contains(l_id);
  bool writing = job->write_set_p()->contains(l_id);
  assert(reading || writing);

  LogicalDataObject* ldo =
    const_cast<LogicalDataObject*>(data_manager_->FindLogicalObject(l_id));

  boost::unique_lock<boost::mutex> lock(ldo->mutex());

  data_version_t version;
  if (reading) {
    if (!job->vmap_read()->query_entry(l_id, &version)) {
      dbg(DBG_ERROR, "ERROR: logical id %lu is not versioned in the read context of %s.\n",
          l_id, job->job_name().c_str());
      exit(-1);
    }
  }

  // Just for checking
  data_version_t unused_version;
  if (writing) {
    if (!job->vmap_write()->query_entry(l_id, &unused_version)) {
      dbg(DBG_ERROR, "ERROR: logical id %lu is not versioned in the write context of %s.\n",
          l_id, job->job_name().c_str());
      exit(-1);
    }
  }

  if (!reading) {
    PhysicalData target_instance;
    GetFreeDataAtWorker(worker, ldo, &target_instance);

    if (job_manager_->CausingUnwantedSerialization(job, l_id, target_instance)) {
      dbg(DBG_SCHED, "Causing unwanted serialization for data %lu.\n", l_id);
    }

    AllocateLdoInstanceToJob(job, ldo, target_instance);
    return true;
  }

  PhysicalDataVector instances_at_worker;
  data_manager_->InstancesByWorkerAndVersion(
      ldo, worker->worker_id(), version, &instances_at_worker);

  JobEntryList list;
  VersionedLogicalData vld(l_id, version);
  // log_.log_StartTimer();
  job_manager_->GetJobsNeedDataVersion(&list, vld);
  // log_.log_StopTimer();
  // std::cout << "versioning: " << log_.timer() << std::endl;
  assert(list.size() >= 1);
  bool writing_needed_version = (list.size() > 1) && writing;


  if (instances_at_worker.size() > 1) {
    PhysicalData target_instance;

    bool found = false;
    PhysicalDataVector::iterator iter;
    for (iter = instances_at_worker.begin(); iter != instances_at_worker.end(); iter++) {
      if (!job_manager_->CausingUnwantedSerialization(job, l_id, *iter)) {
        target_instance = *iter;
        found = true;
        break;
      }
    }

    if (!found) {
      dbg(DBG_SCHED, "Avoiding unwanted serialization for data %lu (1).\n", l_id);
      GetFreeDataAtWorker(worker, ldo, &target_instance);
      LocalCopyData(worker, ldo, &instances_at_worker[0], &target_instance);
    }

    AllocateLdoInstanceToJob(job, ldo, target_instance);
    return true;
  }


  if ((instances_at_worker.size() == 1) && !writing_needed_version) {
    PhysicalData target_instance;

    if (!job_manager_->CausingUnwantedSerialization(job, l_id, instances_at_worker[0])) {
      target_instance = instances_at_worker[0];
    } else {
      dbg(DBG_SCHED, "Avoiding unwanted serialization for data %lu (2).\n", l_id);
      GetFreeDataAtWorker(worker, ldo, &target_instance);
      LocalCopyData(worker, ldo, &instances_at_worker[0], &target_instance);
    }

    AllocateLdoInstanceToJob(job, ldo, target_instance);
    return true;
  }


  if ((instances_at_worker.size() == 1) && writing_needed_version) {
    PhysicalData target_instance;

    if (!job_manager_->CausingUnwantedSerialization(job, l_id, instances_at_worker[0])) {
      target_instance = instances_at_worker[0];
      PhysicalData copy_data;
      GetFreeDataAtWorker(worker, ldo, &copy_data);
      LocalCopyData(worker, ldo, &target_instance, &copy_data);
    } else {
      dbg(DBG_SCHED, "Avoiding unwanted serialization for data %lu (3).\n", l_id);
      GetFreeDataAtWorker(worker, ldo, &target_instance);
      LocalCopyData(worker, ldo, &instances_at_worker[0], &target_instance);
    }

    AllocateLdoInstanceToJob(job, ldo, target_instance);
    return true;
  }


  if ((instances_at_worker.size() == 0) && (version == NIMBUS_INIT_DATA_VERSION)) {
    PhysicalData created_data;
    CreateDataAtWorker(worker, ldo, &created_data);

    AllocateLdoInstanceToJob(job, ldo, created_data);
    return true;
  }

  PhysicalDataVector instances_in_system;
  data_manager_->InstancesByVersion(ldo, version, &instances_in_system);

  if ((instances_at_worker.size() == 0) && (instances_in_system.size() >= 1)) {
    PhysicalData from_instance = instances_in_system[0];
    worker_id_t sender_id = from_instance.worker();
    SchedulerWorker* worker_sender;
    if (!server_->GetSchedulerWorkerById(worker_sender, sender_id)) {
      dbg(DBG_ERROR, "ERROR: could not find worker with id %lu.\n", sender_id);
      exit(-1);
    }

    PhysicalData target_instance;
    GetFreeDataAtWorker(worker, ldo, &target_instance);
    RemoteCopyData(worker_sender, worker, ldo, &from_instance, &target_instance);

    AllocateLdoInstanceToJob(job, ldo, target_instance);
    return true;
  }

  dbg(DBG_ERROR, "ERROR: the version (%lu) of logical data %s (%lu) needed for job %s (%lu) does not exist.\n", // NOLINT
      version, ldo->variable().c_str(), l_id, job->job_name().c_str(), job->job_id());
  assert(instances_in_system.size() >= 1);

  return false;
}


bool JobAssigner::AllocateLdoInstanceToJob(JobEntry* job,
                                           LogicalDataObject* ldo,
                                           PhysicalData pd) {
  assert(job->versioned());
  PhysicalData pd_new = pd;

  if (job->write_set_p()->contains(ldo->id())) {
    data_version_t v_out;
    job->vmap_write()->query_entry(ldo->id(), &v_out);
    pd_new.set_version(v_out);
    pd_new.set_last_job_write(job->job_id());
    pd_new.clear_list_job_read();
    job->before_set_p()->insert(pd.list_job_read());
    job->before_set_p()->insert(pd.last_job_write());
  }

  if (job->read_set_p()->contains(ldo->id())) {
    data_version_t v_in;
    job->vmap_read()->query_entry(ldo->id(), &v_in);
    assert(v_in == pd.version());
    pd_new.add_to_list_job_read(job->job_id());
    job->before_set_p()->insert(pd.last_job_write());
  }

  job->set_physical_table_entry(ldo->id(), pd.id());

  data_manager_->UpdatePhysicalInstance(ldo, pd, pd_new);

  return true;
}

size_t JobAssigner::GetObsoleteLdoInstancesAtWorker(SchedulerWorker* worker,
                                                    LogicalDataObject* ldo,
                                                    PhysicalDataVector* dest) {
  size_t count = 0;
  dest->clear();
  PhysicalDataVector pv;
  data_manager_->InstancesByWorker(ldo, worker->worker_id(), &pv);
  PhysicalDataVector::iterator iter = pv.begin();
  for (; iter != pv.end(); ++iter) {
    JobEntryList list;
    VersionedLogicalData vld(ldo->id(), iter->version());
    // log_.log_StartTimer();
    if (job_manager_->GetJobsNeedDataVersion(&list, vld) == 0) {
      dest->push_back(*iter);
      ++count;
    }
    // log_.log_StopTimer();
    // std::cout << "versioning: " << log_.timer() << std::endl;
  }
  return count;
}

bool JobAssigner::CreateDataAtWorker(SchedulerWorker* worker,
                                     LogicalDataObject* ldo,
                                     PhysicalData* created_data) {
  std::vector<job_id_t> j;
  id_maker_->GetNewJobID(&j, 1);
  std::vector<physical_data_id_t> d;
  id_maker_->GetNewPhysicalDataID(&d, 1);
  IDSet<job_id_t> before;

  // Update the job table.
  job_manager_->AddCreateDataJobEntry(j[0]);

  // Update data table.
  IDSet<job_id_t> list_job_read;
  list_job_read.insert(j[0]);  // if other job wants to write, waits for creation.
  PhysicalData p(d[0], worker->worker_id(), NIMBUS_INIT_DATA_VERSION, list_job_read, j[0]);
  data_manager_->AddPhysicalInstance(ldo, p);

  // send the create command to worker.
  job_manager_->UpdateBeforeSet(&before);
  CreateDataCommand cm(ID<job_id_t>(j[0]),
                       ldo->variable(),
                       ID<logical_data_id_t>(ldo->id()),
                       ID<physical_data_id_t>(d[0]),
                       before);
  server_->SendCommand(worker, &cm);

  *created_data = p;

  return true;
}

bool JobAssigner::RemoteCopyData(SchedulerWorker* from_worker,
                                 SchedulerWorker* to_worker,
                                 LogicalDataObject* ldo,
                                 PhysicalData* from_data,
                                 PhysicalData* to_data) {
  assert(from_worker->worker_id() == from_data->worker());
  assert(to_worker->worker_id() == to_data->worker());

  std::vector<job_id_t> j;
  id_maker_->GetNewJobID(&j, 2);
  job_id_t receive_id = j[0];
  job_id_t send_id = j[1];
  IDSet<job_id_t> before;

  // Receive part

  // Update the job table.
  job_manager_->AddRemoteCopyReceiveJobEntry(receive_id);

  // Update data table.
  PhysicalData to_data_new = *to_data;
  to_data_new.set_version(from_data->version());
  to_data_new.set_last_job_write(receive_id);
  to_data_new.clear_list_job_read();
  data_manager_->UpdatePhysicalInstance(ldo, *to_data, to_data_new);

  // send remote copy receive job to worker.
  before.clear();
  before.insert(to_data->list_job_read());
  before.insert(to_data->last_job_write());
  job_manager_->UpdateBeforeSet(&before);
  RemoteCopyReceiveCommand cm_r(ID<job_id_t>(receive_id),
                                ID<physical_data_id_t>(to_data->id()),
                                before);
  server_->SendCommand(to_worker, &cm_r);


  // Send Part.

  // Update the job table.
  job_manager_->AddRemoteCopySendJobEntry(send_id);

  // Update data table.
  PhysicalData from_data_new = *from_data;
  from_data_new.add_to_list_job_read(send_id);
  data_manager_->UpdatePhysicalInstance(ldo, *from_data, from_data_new);

  // send remote copy send command to worker.
  before.clear();
  before.insert(from_data->last_job_write());
  job_manager_->UpdateBeforeSet(&before);
  RemoteCopySendCommand cm_s(ID<job_id_t>(send_id),
                             ID<job_id_t>(receive_id),
                             ID<physical_data_id_t>(from_data->id()),
                             ID<worker_id_t>(to_worker->worker_id()),
                             to_worker->ip(),
                             ID<port_t>(to_worker->port()),
                             before);
  server_->SendCommand(from_worker, &cm_s);


  *from_data = from_data_new;
  *to_data = to_data_new;

  return true;
}

bool JobAssigner::LocalCopyData(SchedulerWorker* worker,
                                LogicalDataObject* ldo,
                                PhysicalData* from_data,
                                PhysicalData* to_data) {
  assert(worker->worker_id() == from_data->worker());
  assert(worker->worker_id() == to_data->worker());

  std::vector<job_id_t> j;
  id_maker_->GetNewJobID(&j, 1);
  IDSet<job_id_t> before;

  // Update the job table.
  job_manager_->AddLocalCopyJobEntry(j[0]);

  // Update data table.
  PhysicalData from_data_new = *from_data;
  from_data_new.add_to_list_job_read(j[0]);
  data_manager_->UpdatePhysicalInstance(ldo, *from_data, from_data_new);

  PhysicalData to_data_new = *to_data;
  to_data_new.set_version(from_data->version());
  to_data_new.set_last_job_write(j[0]);
  to_data_new.clear_list_job_read();
  data_manager_->UpdatePhysicalInstance(ldo, *to_data, to_data_new);

  // send local copy command to worker.
  before.insert(to_data->list_job_read());
  before.insert(to_data->last_job_write());
  before.insert(from_data->last_job_write());
  job_manager_->UpdateBeforeSet(&before);
  LocalCopyCommand cm_c(ID<job_id_t>(j[0]),
                        ID<physical_data_id_t>(from_data->id()),
                        ID<physical_data_id_t>(to_data->id()),
                        before);
  server_->SendCommand(worker, &cm_c);

  *from_data = from_data_new;
  *to_data = to_data_new;

  return true;
}

bool JobAssigner::GetFreeDataAtWorker(SchedulerWorker* worker,
    LogicalDataObject* ldo, PhysicalData* free_data) {
  PhysicalDataVector obsolete_instances;
  if (GetObsoleteLdoInstancesAtWorker(worker, ldo, &obsolete_instances) > 0) {
    *free_data = obsolete_instances[0];
    return true;
  }

  // Since there are no obsoletes, go ahead and create a new one.
  return CreateDataAtWorker(worker, ldo, free_data);
}

bool JobAssigner::SendComputeJobToWorker(SchedulerWorker* worker,
                                         JobEntry* job) {
  if (job->job_type() == JOB_COMP) {
    ID<job_id_t> job_id(job->job_id());
    ID<job_id_t> future_job_id(job->future_job_id());
    IDSet<physical_data_id_t> read_set, write_set;
    // TODO(omidm): check the return value of the following methods.
    job->GetPhysicalReadSet(&read_set);
    job->GetPhysicalWriteSet(&write_set);
    ComputeJobCommand cm(job->job_name(),
                         job_id,
                         read_set,
                         write_set,
                         job->before_set(),
                         job->after_set(),
                         future_job_id,
                         job->sterile(),
                         job->params());
    dbg(DBG_SCHED, "Sending compute job %lu to worker %lu.\n", job->job_id(), worker->worker_id());
    server_->SendCommand(worker, &cm);
    return true;
  } else {
    dbg(DBG_ERROR, "Job with id %lu is not a compute job.\n", job->job_id());
    return false;
  }
}

}  // namespace nimbus