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
  * A Nimbus worker.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#include <unistd.h>
#include <boost/functional/hash.hpp>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <ctime>
#include <list>
#include <limits>
#include "src/shared/fast_log.h"
#include "src/shared/profiler_malloc.h"
#include "src/worker/worker.h"
#include "src/worker/worker_ldo_map.h"
#include "src/worker/worker_manager.h"
#include "src/worker/util_dumping.h"
#include "src/data/physbam/physbam_data.h"

#define SCHEDULER_COMMAND_GROUP_QUOTA 10
#define RECEIVE_EVENT_BATCH_QUOTA 100000

using boost::hash;

namespace nimbus {

Worker::Worker(std::string scheduler_ip, port_t scheduler_port,
    port_t listening_port, Application* a)
: scheduler_ip_(scheduler_ip),
  scheduler_port_(scheduler_port),
  listening_port_(listening_port),
  application_(a) {
    {
      struct sched_param param;
      param.sched_priority = 0;
      int st = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
      if (st != 0) {
        // Scheduling setting goes wrong.
        exit(1);
      }
    }
    id_ = -1;
    ip_address_ = NIMBUS_RECEIVER_KNOWN_IP;
    execution_template_active_ = true;
    cache_manager_active_ = true;
    vdata_manager_active_ = true;
    worker_manager_ = new WorkerManager();
    ddb_ = new DistributedDB();
    DUMB_JOB_ID = std::numeric_limits<job_id_t>::max();
    filling_execution_template_ = false;
    prepare_rewind_phase_ = false;
    worker_job_graph_.AddVertex(
        DUMB_JOB_ID,
        new WorkerJobEntry(
            DUMB_JOB_ID, NULL, WorkerJobEntry::CONTROL));
}

Worker::~Worker() {
  WorkerJobVertex* vertex = NULL;
  worker_job_graph_.GetVertex(DUMB_JOB_ID, &vertex);
  delete vertex->entry();
  worker_job_graph_.RemoveVertex(DUMB_JOB_ID);
  delete worker_manager_;
  delete ddb_;
  delete receive_event_mutex_;
  delete receive_event_cond_;
  delete command_processor_mutex_;
  delete command_processor_cond_;
}

void Worker::Run() {
  CreateModules();

  SetupTimers();
  SetupApplication();
  SetupWorkerManager();
  SetupSchedulerClient();
  SetupWorkerDataExchanger();

  // SetupCommandProcessor();
  command_processor_thread_ = new boost::thread(
      boost::bind(&Worker::SetupCommandProcessor, this));

  // SetupReceiveEventProcessor();
  receive_event_thread_ = new boost::thread(
      boost::bind(&Worker::SetupReceiveEventProcessor, this));

  WorkerCoreProcessor();
}


void Worker::CreateModules() {
  id_maker_ = new IDMaker();
  ldo_map_ = new WorkerLdoMap();
  client_ = new SchedulerClient(scheduler_ip_, scheduler_port_);
  data_exchanger_ = new WorkerDataExchanger(listening_port_);
  receive_event_mutex_ = new boost::recursive_mutex();
  receive_event_cond_ = new boost::condition_variable_any();
  command_processor_mutex_ = new boost::recursive_mutex();
  command_processor_cond_ = new boost::condition_variable_any();
}

void Worker::SetupTimers() {
  timer::InitializeKeys();
  timer::InitializeTimers();

  stat_blocked_job_num_ = 0;
  stat_ready_job_num_ = 0;
  stat_busy_cores_ = 0;
  stat_blocked_cores_ = 0;
  stat_idle_cores_ = WorkerManager::across_job_parallism;
  run_timer_.set_name("kSumCyclesRun");
  block_timer_.set_name("kSumCyclesBlock");
  total_timer_.set_name("kSumCyclesTotal");

  // timer::StartTimer(timer::kSumCyclesTotal, WorkerManager::across_job_parallism);
  total_timer_.Start(WorkerManager::across_job_parallism);
}

void Worker::SetupApplication() {
  application_->set_cache_manager_active(cache_manager_active_);
  application_->set_vdata_manager_active(vdata_manager_active_);
  application_->Start(client_, id_maker_, ldo_map_);
}

void Worker::SetupWorkerManager() {
  worker_manager_->worker_ = this;
  dbg(DBG_WORKER_FD, DBG_WORKER_FD_S"Launching worker threads.\n");
  worker_manager_->StartWorkerThreads();
  dbg(DBG_WORKER_FD, DBG_WORKER_FD_S"Finishes launching worker threads.\n");
  worker_manager_->TriggerScheduling();
}

void Worker::SetupSchedulerClient() {
  LoadSchedulerCommands();
  client_->set_command_processor_mutex(command_processor_mutex_);
  client_->set_command_processor_cond(command_processor_cond_);
  client_->set_scheduler_command_table(&scheduler_command_table_);
  client_->set_execution_template_active(execution_template_active_);
  client_thread_ = new boost::thread(
      boost::bind(&SchedulerClient::Run, client_));
}

void Worker::SetupWorkerDataExchanger() {
  data_exchanger_->set_receive_event_mutex(receive_event_mutex_);
  data_exchanger_->set_receive_event_cond(receive_event_cond_);
  data_exchanger_->Run();
}

void Worker::SetupCommandProcessor() {
  timer::InitializeTimers();
  RunCommandProcessor();
}

void Worker::RunCommandProcessor() {
  while (true) {
    SchedulerCommandList storage;
    {
      boost::unique_lock<boost::recursive_mutex> lock(*command_processor_mutex_);

      while (!client_->ReceiveCommands(&storage, SCHEDULER_COMMAND_GROUP_QUOTA)) {
        command_processor_cond_->wait(lock);
      }
    }

    SchedulerCommandList::iterator iter = storage.begin();
    for (; iter != storage.end(); ++iter) {
      timer::StartTimer(timer::kCoreCommand);
      SchedulerCommand *comm = *iter;
      dbg(DBG_WORKER, "Receives command from scheduler: %s\n",
          comm->ToString().c_str());
      dbg(DBG_WORKER_FD,
          DBG_WORKER_FD_S"Scheduler command arrives(%s).\n",
          comm->name().c_str());
      ProcessSchedulerCommand(comm);
      delete comm;
      timer::StopTimer(timer::kCoreCommand);
    }
  }
}

void Worker::SetupReceiveEventProcessor() {
  timer::InitializeTimers();
  RunReceiveEventProcessor();
}

void Worker::RunReceiveEventProcessor() {
  while (true) {
    WorkerDataExchanger::EventList events;
    {
      boost::unique_lock<boost::recursive_mutex> lock(*receive_event_mutex_);

      while (data_exchanger_->PullReceiveEvents(&events, RECEIVE_EVENT_BATCH_QUOTA) == 0) {
        receive_event_cond_->wait(lock);
      }
    }
    timer::StartTimer(timer::kJobGraph1);
    boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
    timer::StartTimer(timer::kCoreTransmission);
    ProcessReceiveEvents(events);
    timer::StopTimer(timer::kCoreTransmission);
    timer::StopTimer(timer::kJobGraph1);
  }
}

void Worker::WorkerCoreProcessor() {
  receive_event_thread_->join();

  assert(false);

  while (true) {
    bool processed_tasks = false;
    // Process command.
    SchedulerCommandList storage;
    client_->ReceiveCommands(&storage, SCHEDULER_COMMAND_GROUP_QUOTA);
    SchedulerCommandList::iterator iter = storage.begin();
    for (; iter != storage.end(); ++iter) {
      timer::StartTimer(timer::kCoreCommand);
      SchedulerCommand *comm = *iter;
      dbg(DBG_WORKER, "Receives command from scheduler: %s\n",
          comm->ToString().c_str());
      dbg(DBG_WORKER_FD,
          DBG_WORKER_FD_S"Scheduler command arrives(%s).\n",
          comm->name().c_str());
      processed_tasks = true;
      ProcessSchedulerCommand(comm);
      delete comm;
      timer::StopTimer(timer::kCoreCommand);
    }

    // Poll receive events from data exchanger.
    {
      timer::StartTimer(timer::kJobGraph1);
      boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
      WorkerDataExchanger::EventList events;
      size_t count = data_exchanger_->PullReceiveEvents(&events, RECEIVE_EVENT_BATCH_QUOTA);
        if (count > 0) {
          processed_tasks = true;
          timer::StartTimer(timer::kCoreTransmission);
          ProcessReceiveEvents(events);
          timer::StopTimer(timer::kCoreTransmission);
        }
      timer::StopTimer(timer::kJobGraph1);
    }

    // Job done processing.
    // JobList local_job_done_list;
    // worker_manager_->GetLocalJobDoneList(&local_job_done_list);
    // quota = 10;
    // while (!local_job_done_list.empty()) {
    //   timer::StartTimer(timer::kCoreJobDone);
    //   Job* job = local_job_done_list.front();
    //   local_job_done_list.pop_front();
    //   process_jobs = true;
    //   NotifyLocalJobDone(job);
    //   timer::StopTimer(timer::kCoreJobDone);
    //   if (--quota <= 0) {
    //     break;
    //   }
    // }

    if (!processed_tasks) {
//      typename WorkerJobVertex::Iter iter = worker_job_graph_.begin();
//      for (; iter != worker_job_graph_.end(); ++iter) {
//        if (iter->second->incoming_edges()->size() != 0) {
//          Job* job = iter->second->entry()->get_job();
//          std::string name = job->name();
//          if (name.find("Copy") == std::string::npos &&
//              name.find("extrapolate_phi") != std::string::npos) {
//              std::cout << "OMID: " << job->id().elem() << " "
//                << iter->second->incoming_edges()->size() << " ";
//            WorkerJobEdge::Map::iterator it = iter->second->incoming_edges()->begin();
//            for (; it != iter->second->incoming_edges()->end(); ++it) {
//              WorkerJobEdge* edge = it->second;
//              std::cout << edge->start_vertex()->entry()->get_job_id() << " ";
//            }
//            std::cout << job->before_set().ToNetworkData() << std::endl;
//          }
//        }
//      }

      usleep(10);
    }
  }
}

// Extracts data objects from the read/write/scratch/reduce set to data array.
void Worker::ResolveDataArray(Job* job) {
  dbg(DBG_WORKER_FD, DBG_WORKER_FD_S"Job(name %s, #%d) ready to run.\n",
      job->name().c_str(), job->id().elem());
  job->data_array.clear();
  if ((dynamic_cast<CreateDataJob*>(job) != NULL)) {  // NOLINT
    assert(job->get_read_set().size() == 0);
    assert(job->get_scratch_set().size() == 0);
    assert(job->get_reduce_set().size() == 0);
    assert(job->get_write_set().size() == 1);
    job->data_array.push_back(
        data_map_.AcquireAccess(*job->write_set().begin(), job->id().elem(),
                                PhysicalDataMap::INIT));
  } else if ((dynamic_cast<MegaRCRJob*>(job) != NULL)) {  // NOLINT
    MegaRCRJob *mega_job = reinterpret_cast<MegaRCRJob*>(job);
    std::vector<physical_data_id_t>::const_iterator iter = mega_job->to_phy_ids_p()->begin();
    for (; iter != mega_job->to_phy_ids_p()->end(); ++iter) {
      job->data_array.push_back(
          data_map_.AcquireAccess(*iter, job->id().elem(), PhysicalDataMap::WRITE));
    }
  } else {
    IDSet<physical_data_id_t>::IDSetIter iter;

    const IDSet<physical_data_id_t>& read = job->get_read_set();
    for (iter = read.begin(); iter != read.end(); iter++) {
      job->data_array.push_back(
          data_map_.AcquireAccess(*iter, job->id().elem(), PhysicalDataMap::READ));
    }

    const IDSet<physical_data_id_t>& reduce = job->get_reduce_set();
    for (iter = reduce.begin(); iter != reduce.end(); iter++) {
      job->data_array.push_back(
          data_map_.AcquireAccess(*iter, job->id().elem(), PhysicalDataMap::REDUCE));
    }

    const IDSet<physical_data_id_t>& write = job->get_write_set();
    for (iter = write.begin(); iter != write.end(); iter++) {
      job->data_array.push_back(
          data_map_.AcquireAccess(*iter, job->id().elem(), PhysicalDataMap::WRITE));
    }

    const IDSet<physical_data_id_t>& scratch = job->get_scratch_set();
    for (iter = scratch.begin(); iter != scratch.end(); iter++) {
      job->data_array.push_back(
          data_map_.AcquireAccess(*iter, job->id().elem(), PhysicalDataMap::SCRATCH));
    }
  }
}

void Worker::ProcessSchedulerCommand(SchedulerCommand* cm) {
  switch (cm->type()) {
    case SchedulerCommand::HANDSHAKE:
      ProcessHandshakeCommand(reinterpret_cast<HandshakeCommand*>(cm));
      break;
    case SchedulerCommand::JOB_DONE:
      ProcessJobDoneCommand(reinterpret_cast<JobDoneCommand*>(cm));
      break;
    case SchedulerCommand::EXECUTE_COMPUTE:
      ProcessComputeJobCommand(reinterpret_cast<ComputeJobCommand*>(cm));
      break;
    case SchedulerCommand::EXECUTE_COMBINE:
      ProcessCombineJobCommand(reinterpret_cast<CombineJobCommand*>(cm));
      break;
    case SchedulerCommand::CREATE_DATA:
      ProcessCreateDataCommand(reinterpret_cast<CreateDataCommand*>(cm));
      break;
    case SchedulerCommand::REMOTE_SEND:
      ProcessRemoteCopySendCommand(reinterpret_cast<RemoteCopySendCommand*>(cm));
      break;
    case SchedulerCommand::REMOTE_RECEIVE:
      ProcessRemoteCopyReceiveCommand(reinterpret_cast<RemoteCopyReceiveCommand*>(cm));
      break;
    case SchedulerCommand::MEGA_RCR:
      ProcessMegaRCRCommand(reinterpret_cast<MegaRCRCommand*>(cm));
      break;
    case SchedulerCommand::LOCAL_COPY:
      ProcessLocalCopyCommand(reinterpret_cast<LocalCopyCommand*>(cm));
      break;
    case SchedulerCommand::LDO_ADD:
      ProcessLdoAddCommand(reinterpret_cast<LdoAddCommand*>(cm));
      break;
    case SchedulerCommand::LDO_REMOVE:
      ProcessLdoRemoveCommand(reinterpret_cast<LdoRemoveCommand*>(cm));
      break;
    case SchedulerCommand::PARTITION_ADD:
      ProcessPartitionAddCommand(reinterpret_cast<PartitionAddCommand*>(cm));
      break;
    case SchedulerCommand::PARTITION_REMOVE:
      ProcessPartitionRemoveCommand(reinterpret_cast<PartitionRemoveCommand*>(cm));
      break;
    case SchedulerCommand::TERMINATE:
      ProcessTerminateCommand(reinterpret_cast<TerminateCommand*>(cm));
      break;
    case SchedulerCommand::DEFINED_TEMPLATE:
      ProcessDefinedTemplateCommand(reinterpret_cast<DefinedTemplateCommand*>(cm));
      break;
    case SchedulerCommand::SAVE_DATA:
      ProcessSaveDataCommand(reinterpret_cast<SaveDataCommand*>(cm));
      break;
    case SchedulerCommand::LOAD_DATA:
      ProcessLoadDataCommand(reinterpret_cast<LoadDataCommand*>(cm));
      break;
    case SchedulerCommand::PREPARE_REWIND:
      ProcessPrepareRewindCommand(reinterpret_cast<PrepareRewindCommand*>(cm));
      break;
    case SchedulerCommand::REQUEST_STAT:
      ProcessRequestStatCommand(reinterpret_cast<RequestStatCommand*>(cm));
      break;
    case SchedulerCommand::PRINT_STAT:
      ProcessPrintStatCommand(reinterpret_cast<PrintStatCommand*>(cm));
      break;
    case SchedulerCommand::START_COMMAND_TEMPLATE:
      ProcessStartCommandTemplateCommand(reinterpret_cast<StartCommandTemplateCommand*>(cm));
      break;
    case SchedulerCommand::END_COMMAND_TEMPLATE:
      ProcessEndCommandTemplateCommand(reinterpret_cast<EndCommandTemplateCommand*>(cm));
      break;
    case SchedulerCommand::SPAWN_COMMAND_TEMPLATE:
      ProcessSpawnCommandTemplateCommand(reinterpret_cast<SpawnCommandTemplateCommand*>(cm));
      break;
    default:
      std::cout << "ERROR: " << cm->ToNetworkData() <<
        " have not been implemented in ProcessSchedulerCommand yet." <<
        std::endl;
      exit(-1);
  }
}

// Processes handshake command. Configures the worker based on the handshake
// command and responds by sending another handshake command.
void Worker::ProcessHandshakeCommand(HandshakeCommand* cm) {
  double time = Log::GetRawTime();
  ID<port_t> port(listening_port_);
  HandshakeCommand new_cm = HandshakeCommand(cm->worker_id(),
      // boost::asio::ip::host_name(), port);
      // "127.0.0.1", port);
      ip_address_, port, time);
  client_->SendCommand(&new_cm);

  if (ip_address_ == NIMBUS_RECEIVER_KNOWN_IP) {
    ip_address_ = cm->ip();
  }
  id_ = cm->worker_id().elem();
  id_maker_->Initialize(id_);
  ddb_->Initialize(ip_address_, id_);

  std::string wstr = int2string(id_);
}

// Processes jobdone command. Moves a job from blocked queue to ready queue if
// its before set is satisfied.
void Worker::ProcessJobDoneCommand(JobDoneCommand* cm) {
  NotifyJobDone(cm->job_id().elem(), cm->final());
}

// Processes computejob command. Generates the corresponding job and pushes it
// to the blocking queue.
void Worker::ProcessComputeJobCommand(ComputeJobCommand* cm) {
  Job* job = application_->CloneJob(cm->job_name());
  job->set_name("Compute:" + cm->job_name());
  job->set_id(cm->job_id());
  job->set_read_set(cm->read_set());
  job->set_write_set(cm->write_set());
  job->set_scratch_set(cm->scratch_set());
  job->set_reduce_set(cm->reduce_set());
  job->set_after_set(cm->after_set());
  job->set_future_job_id(cm->future_job_id());
  job->set_sterile(cm->sterile());
  job->set_region(cm->region());
  job->set_parameters(cm->params());

  if (cm->extra_dependency_p()->size() != 0) {
    IDSet<job_id_t> extended_before_set = cm->before_set();
    IDSet<job_id_t>::ConstIter iter = cm->extra_dependency_p()->begin();
    for (; iter != cm->extra_dependency_p()->end(); ++iter) {
      extended_before_set.insert(*iter);
    }
    job->set_before_set(extended_before_set);
  } else {
    job->set_before_set(cm->before_set());
  }

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddComputeJobTemplate(cm, application_);
  }

  AddJobToGraph(job);
}

void Worker::ProcessCombineJobCommand(CombineJobCommand* cm) {
  Job* job = application_->CloneJob(cm->job_name());
  job->set_name("Combine:" + cm->job_name());
  job->set_id(cm->job_id());
  job->set_scratch_set(cm->scratch_set());
  job->set_reduce_set(cm->reduce_set());
  job->set_region(cm->region());

  if (cm->extra_dependency_p()->size() != 0) {
    IDSet<job_id_t> extended_before_set = cm->before_set();
    IDSet<job_id_t>::ConstIter iter = cm->extra_dependency_p()->begin();
    for (; iter != cm->extra_dependency_p()->end(); ++iter) {
      extended_before_set.insert(*iter);
    }
    job->set_before_set(extended_before_set);
  } else {
    job->set_before_set(cm->before_set());
  }

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddCombineJobTemplate(cm, application_);
  }

  AddJobToGraph(job);
}

// Processes createdata command. Generates the corresponding data and pushes a
// data creation job to the blocking queue.
void Worker::ProcessCreateDataCommand(CreateDataCommand* cm) {
  Data * data = application_->CloneData(cm->data_name());
  data->set_logical_id(cm->logical_data_id().elem());
  data->set_physical_id(cm->physical_data_id().elem());
  // data->set_name(cm->data_name());
  const LogicalDataObject* ldo;
  ldo = ldo_map_->FindLogicalObject(cm->logical_data_id().elem());
  data->set_region(*(ldo->region()));

  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  data_map_.AddMapping(data->physical_id(), data);

  Job * job = new CreateDataJob();
  job->set_name("CreateData:" + cm->data_name());
  job->set_id(cm->job_id());
  IDSet<physical_data_id_t> write_set;
  write_set.insert(cm->physical_data_id().elem());
  job->set_write_set(write_set);
  job->set_before_set(cm->before_set());
  AddJobToGraph(job);
}

void Worker::ProcessRemoteCopySendCommand(RemoteCopySendCommand* cm) {
  RemoteCopySendJob * job = new RemoteCopySendJob(data_exchanger_, application_);
  data_exchanger_->AddContactInfo(cm->to_worker_id().elem(),
      cm->to_ip(), cm->to_port().elem());
  job->set_name("RemoteCopySend");
  job->set_id(cm->job_id());
  job->set_receive_job_id(cm->receive_job_id());
  job->set_mega_rcr_job_id(cm->mega_rcr_job_id());
  job->set_to_worker_id(cm->to_worker_id());
  job->set_to_ip(cm->to_ip());
  job->set_to_port(cm->to_port());
  IDSet<physical_data_id_t> read_set;
  read_set.insert(cm->from_physical_data_id().elem());
  job->set_read_set(read_set);

  if (cm->extra_dependency_p()->size() != 0) {
    IDSet<job_id_t> extended_before_set = cm->before_set();
    IDSet<job_id_t>::ConstIter iter = cm->extra_dependency_p()->begin();
    for (; iter != cm->extra_dependency_p()->end(); ++iter) {
      extended_before_set.insert(*iter);
    }
    job->set_before_set(extended_before_set);
  } else {
    job->set_before_set(cm->before_set());
  }

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddRemoteCopySendJobTemplate(cm, application_, data_exchanger_);
  }

  AddJobToGraph(job);
}

void Worker::ProcessRemoteCopyReceiveCommand(RemoteCopyReceiveCommand* cm) {
  Job * job = new RemoteCopyReceiveJob(application_);
  job->set_name("RemoteCopyReceive");
  job->set_id(cm->job_id());
  IDSet<physical_data_id_t> write_set;
  write_set.insert(cm->to_physical_data_id().elem());
  job->set_write_set(write_set);

  if (cm->extra_dependency_p()->size() != 0) {
    IDSet<job_id_t> extended_before_set = cm->before_set();
    IDSet<job_id_t>::ConstIter iter = cm->extra_dependency_p()->begin();
    for (; iter != cm->extra_dependency_p()->end(); ++iter) {
      extended_before_set.insert(*iter);
    }
    job->set_before_set(extended_before_set);
  } else {
    job->set_before_set(cm->before_set());
  }

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddRemoteCopyReceiveJobTemplate(cm, application_);
  }

  AddJobToGraph(job);
}

void Worker::ProcessMegaRCRCommand(MegaRCRCommand* cm) {
  Job * job = new MegaRCRJob(application_,
                             cm->receive_job_ids(),
                             cm->to_physical_data_ids());
  job->set_name("MegaRCR");
  job->set_id(cm->job_id());

  job->set_before_set(cm->extra_dependency());

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddMegaRCRJobTemplate(cm, application_);
  }

  AddJobToGraph(job);
}

void Worker::ProcessSaveDataCommand(SaveDataCommand* cm) {
  SaveDataJob * job = new SaveDataJob(ddb_, application_);
  job->set_name("SaveData");
  job->set_id(cm->job_id());
  job->set_checkpoint_id(cm->checkpoint_id().elem());
  IDSet<physical_data_id_t> read_set;
  read_set.insert(cm->from_physical_data_id().elem());
  job->set_read_set(read_set);
  job->set_before_set(cm->before_set());
  AddJobToGraph(job);
}

void Worker::ProcessLoadDataCommand(LoadDataCommand* cm) {
  LoadDataJob * job = new LoadDataJob(ddb_, application_);
  job->set_name("LoadData");
  job->set_id(cm->job_id());
  job->set_handle(cm->handle());
  IDSet<physical_data_id_t> write_set;
  write_set.insert(cm->to_physical_data_id().elem());
  job->set_write_set(write_set);
  job->set_before_set(cm->before_set());
  AddJobToGraph(job);
}

void Worker::ProcessPrepareRewindCommand(PrepareRewindCommand* cm) {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  // Remove all blocked jobs from regular job graph.
  ClearBlockedJobs();
  // Setting this flag stops adding more ready jobs from execution templates.
  prepare_rewind_phase_ = true;

  // Wait untill all runing jobs finish.
  while (!AllReadyJobsAreDone()) {
    job_graph_cond_.wait(lock);
  }

  // Clear the obsolete state.
  pending_events_.clear();
  active_execution_templates_.clear();
  prepare_rewind_phase_ = false;

  PrepareRewindCommand command(ID<worker_id_t>(id_), cm->checkpoint_id());
  client_->SendCommand(&command);
}

void Worker::ProcessRequestStatCommand(RequestStatCommand* cm) {
  int64_t idle, block, run;
  GetTimerStat(&idle, &block, &run);
  RespondStatCommand command(cm->query_id(), id_, run, block, idle);
  client_->SendCommand(&command);
}

void Worker::ProcessPrintStatCommand(PrintStatCommand* cm) {
  PrintTimerStat();
}

void Worker::ProcessLocalCopyCommand(LocalCopyCommand* cm) {
  Job * job = new LocalCopyJob(application_);
  job->set_name("LocalCopy");
  job->set_id(cm->job_id());
  IDSet<physical_data_id_t> read_set;
  read_set.insert(cm->from_physical_data_id().elem());
  job->set_read_set(read_set);
  IDSet<physical_data_id_t> write_set;
  write_set.insert(cm->to_physical_data_id().elem());
  job->set_write_set(write_set);

  if (cm->extra_dependency_p()->size() != 0) {
    IDSet<job_id_t> extended_before_set = cm->before_set();
    IDSet<job_id_t>::ConstIter iter = cm->extra_dependency_p()->begin();
    for (; iter != cm->extra_dependency_p()->end(); ++iter) {
      extended_before_set.insert(*iter);
    }
    job->set_before_set(extended_before_set);
  } else {
    job->set_before_set(cm->before_set());
  }

  if (filling_execution_template_) {
    std::map<std::string, ExecutionTemplate*>::iterator iter =
      execution_templates_.find(execution_template_in_progress_);
    assert(iter != execution_templates_.end());
    iter->second->AddLocalCopyJobTemplate(cm, application_);
  }

  AddJobToGraph(job);
}

void Worker::ProcessLdoAddCommand(LdoAddCommand* cm) {
    const LogicalDataObject* ldo = cm->object();
    if (!ldo_map_->AddLogicalObject(ldo->id(), ldo->variable(), *(ldo->region()) ) )
        dbg(DBG_ERROR, "Worker could not add logical object %i to ldo map\n", (ldo->id()));
}

void Worker::ProcessLdoRemoveCommand(LdoRemoveCommand* cm) {
    const LogicalDataObject* ldo = cm->object();
    if (!ldo_map_->RemoveLogicalObject(ldo->id()))
        dbg(DBG_ERROR, "Worker could not remove logical object %i to ldo map\n", (ldo->id()));
}

void Worker::ProcessPartitionAddCommand(PartitionAddCommand* cm) {
    GeometricRegion r = *(cm->region());
    if (!ldo_map_->AddPartition(cm->id().elem(), r))
        dbg(DBG_ERROR, "Worker could not add partition %i to ldo map\n", cm->id().elem());
}

void Worker::ProcessPartitionRemoveCommand(PartitionRemoveCommand* cm) {
  if (!ldo_map_->RemovePartition(cm->id().elem()))
    dbg(DBG_ERROR, "Worker could not remove partition %i from ldo map\n", cm->id().elem());
}

void Worker::ProcessTerminateCommand(TerminateCommand* cm) {
  // profiler_thread_->interrupt();
  // profiler_thread_->join();
  std::string file_name = int2string(id_) + "_time_per_thread.txt";
  FILE* temp = fopen(file_name.c_str(), "w");
  total_timer_.Print(temp);
  block_timer_.Print(temp);
  run_timer_.Print(temp);
  timer::PrintTimerSummary(temp);
  fclose(temp);
  exit(cm->exit_status().elem());
}

void Worker::ProcessDefinedTemplateCommand(DefinedTemplateCommand* cm) {
  application_->DefinedTemplate(cm->job_graph_name());
}


void Worker::ProcessStartCommandTemplateCommand(StartCommandTemplateCommand* command) {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  assert(!filling_execution_template_);
  std::string key = command->command_template_name();
  std::map<std::string, ExecutionTemplate*>::iterator iter =
    execution_templates_.find(key);
  assert(iter == execution_templates_.end());
  execution_templates_[key] =
    new ExecutionTemplate(key,
                          command->inner_job_ids(),
                          command->outer_job_ids(),
                          command->phy_ids(),
                          application_,
                          data_exchanger_);

  execution_template_in_progress_ = key;
  filling_execution_template_ = true;
}

void Worker::ProcessEndCommandTemplateCommand(EndCommandTemplateCommand* command) {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  assert(filling_execution_template_);
  std::string key = command->command_template_name();
  std::map<std::string, ExecutionTemplate*>::iterator iter =
    execution_templates_.find(key);
  assert(iter != execution_templates_.end());
  iter->second->Finalize();

  filling_execution_template_ = false;
}

void Worker::ProcessSpawnCommandTemplateCommand(SpawnCommandTemplateCommand* command) {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);

  std::string key = command->command_template_name();
  std::map<std::string, ExecutionTemplate*>::iterator iter =
    execution_templates_.find(key);
  assert(iter != execution_templates_.end());
  ExecutionTemplate *et = iter->second;

  // Prune the extra dependency.
  IDSet<job_id_t> extra_dependency;
  {
    IDSet<job_id_t>::ConstIter it = command->extra_dependency_p()->begin();
    for (; it != command->extra_dependency_p()->end(); ++it) {
      job_id_t before_job_id = *it;
      // Only copy jobs are in extra dependency. -omidm
      assert(IDMaker::SchedulerProducedJobID(before_job_id));
      // TODO(omidm): what if this is too small, controller do not support after
      // map and before set clean up for worker template!!!
      if (InFinishHintSet(before_job_id)) {
        continue;
      }
      if (worker_job_graph_.HasVertex(before_job_id)) {
        WorkerJobVertex* before_job_vertex = NULL;
        worker_job_graph_.GetVertex(before_job_id, &before_job_vertex);
        if (before_job_vertex->entry()->get_state() != WorkerJobEntry::FINISH) {
          extra_dependency.insert(before_job_id);
        }
      }
    }
  }

  JobList ready_jobs;
  template_id_t tgi = command->template_generation_id();
  EventMap::iterator it = pending_events_.find(tgi);
  if (it != pending_events_.end()) {
    bool instantiated =
      et->Instantiate(command->inner_job_ids(),
                      command->outer_job_ids(),
                      extra_dependency,
                      command->parameters(),
                      command->phy_ids(),
                      it->second,
                      tgi,
                      command->extensions(),
                      &ready_jobs);
    if (instantiated) {
      pending_events_.erase(it);
    }
  } else {
    WorkerDataExchanger::EventList empty_pending_events;
    et->Instantiate(command->inner_job_ids(),
                    command->outer_job_ids(),
                    extra_dependency,
                    command->parameters(),
                    command->phy_ids(),
                    empty_pending_events,
                    tgi,
                    command->extensions(),
                    &ready_jobs);
  }

  // If the instantiation is pending, don't do the rest! -omidm
  if (!et->pending_instantiate()) {
    StatAddJob(et->job_num());

    active_execution_templates_[tgi] = iter->second;

    JobList::iterator i = ready_jobs.begin();
    for (; i != ready_jobs.end(); ++i) {
      ResolveDataArray(*i);
    }
    bool success_flag = worker_manager_->PushJobList(&ready_jobs);
    assert(success_flag);
  }
}


void Worker::LoadSchedulerCommands() {
  // std::stringstream cms("runjob killjob haltjob resumejob jobdone createdata copydata deletedata");   // NOLINT
  scheduler_command_table_[SchedulerCommand::HANDSHAKE] = new HandshakeCommand();
  scheduler_command_table_[SchedulerCommand::JOB_DONE] = new JobDoneCommand();
  scheduler_command_table_[SchedulerCommand::EXECUTE_COMPUTE] = new ComputeJobCommand();
  scheduler_command_table_[SchedulerCommand::EXECUTE_COMBINE] = new CombineJobCommand();
  scheduler_command_table_[SchedulerCommand::CREATE_DATA] = new CreateDataCommand();
  scheduler_command_table_[SchedulerCommand::REMOTE_SEND] = new RemoteCopySendCommand();
  scheduler_command_table_[SchedulerCommand::REMOTE_RECEIVE] = new RemoteCopyReceiveCommand(); // NOLINT
  scheduler_command_table_[SchedulerCommand::MEGA_RCR] = new MegaRCRCommand();
  scheduler_command_table_[SchedulerCommand::LOCAL_COPY] = new LocalCopyCommand();
  scheduler_command_table_[SchedulerCommand::LDO_ADD] = new LdoAddCommand();
  scheduler_command_table_[SchedulerCommand::LDO_REMOVE] = new LdoRemoveCommand();
  scheduler_command_table_[SchedulerCommand::PARTITION_ADD] = new PartitionAddCommand();
  scheduler_command_table_[SchedulerCommand::PARTITION_REMOVE] = new PartitionRemoveCommand();
  scheduler_command_table_[SchedulerCommand::TERMINATE] = new TerminateCommand();
  scheduler_command_table_[SchedulerCommand::DEFINED_TEMPLATE] = new DefinedTemplateCommand();
  scheduler_command_table_[SchedulerCommand::SAVE_DATA] = new SaveDataCommand();
  scheduler_command_table_[SchedulerCommand::LOAD_DATA] = new LoadDataCommand();
  scheduler_command_table_[SchedulerCommand::PREPARE_REWIND] = new PrepareRewindCommand();
  scheduler_command_table_[SchedulerCommand::START_COMMAND_TEMPLATE] = new StartCommandTemplateCommand(); // NOLINT
  scheduler_command_table_[SchedulerCommand::END_COMMAND_TEMPLATE] = new EndCommandTemplateCommand(); //NOLINT
  scheduler_command_table_[SchedulerCommand::SPAWN_COMMAND_TEMPLATE] = new SpawnCommandTemplateCommand(); // NOLINT
  scheduler_command_table_[SchedulerCommand::REQUEST_STAT] = new RequestStatCommand();
  scheduler_command_table_[SchedulerCommand::PRINT_STAT] = new PrintStatCommand();
}

worker_id_t Worker::id() {
  return id_;
}

void Worker::set_id(worker_id_t id) {
  id_ = id;
}

void Worker::set_ip_address(std::string ip) {
  ip_address_ = ip;
}

void Worker::set_execution_template_active(bool flag) {
  execution_template_active_ = flag;
}

void Worker::set_cache_manager_active(bool flag) {
  cache_manager_active_ = flag;
}

void Worker::set_vdata_manager_active(bool flag) {
  vdata_manager_active_ = flag;
}

PhysicalDataMap* Worker::data_map() {
  return &data_map_;
}

void Worker::AddJobToGraph(Job* job) {
  timer::StartTimer(timer::kJobGraph2);
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);

  // TODO(quhang): when a job is received.
  StatAddJob(1);
  assert(job != NULL);
  job_id_t job_id = job->id().elem();
  dbg(DBG_WORKER_FD,
      DBG_WORKER_FD_S"Job(%s, #%d) is added to the local job graph.\n",
      job->name().c_str(), job_id);
  assert(job_id != DUMB_JOB_ID);

  // Add vertex for the new job.
  WorkerJobVertex* vertex = NULL;
  if (worker_job_graph_.HasVertex(job_id)) {
    // The job is in the graph but not received.
    worker_job_graph_.GetVertex(job_id, &vertex);
    assert(vertex->entry()->get_job() == NULL);
    switch (vertex->entry()->get_state()) {
      case WorkerJobEntry::PENDING: {
        // If the job is a (mega) receive job, add a dumb edge.
        if (dynamic_cast<RemoteCopyReceiveJob*>(job) ||  // NOLINT
            dynamic_cast<MegaRCRJob*>(job)) {  // NOLINT
          worker_job_graph_.AddEdge(DUMB_JOB_ID, job_id);
        }
        break;
      }
      case WorkerJobEntry::PENDING_DATA_RECEIVED: {
        // Flag shows that the data is already received.
        RemoteCopyReceiveJob* receive_job
          = dynamic_cast<RemoteCopyReceiveJob*>(job);  // NOLINT
        assert(receive_job != NULL);
        receive_job->set_data_version(vertex->entry()->get_version());
        receive_job->set_serialized_data(vertex->entry()->get_ser_data());
        break;
      }
      case WorkerJobEntry::PENDING_MEGA_DATA_RECEIVED: {
        MegaRCRJob* mega_receive_job = dynamic_cast<MegaRCRJob*>(job);  // NOLINT
        assert(mega_receive_job != NULL);
        mega_receive_job->set_serialized_data_map(vertex->entry()->ser_data_map());
        if (!mega_receive_job->AllDataReceived()) {
          worker_job_graph_.AddEdge(DUMB_JOB_ID, job_id);
        }
        break;
      }
      default:
        assert(false);
    }
  } else {
    // The job is new.
    worker_job_graph_.AddVertex(job_id, new WorkerJobEntry());
    worker_job_graph_.GetVertex(job_id, &vertex);
    if (dynamic_cast<RemoteCopyReceiveJob*>(job) ||  // NOLINT
        dynamic_cast<MegaRCRJob*>(job)) {  // NOLINT
      worker_job_graph_.AddEdge(DUMB_JOB_ID, job_id);
    }
  }

  vertex->entry()->set_job_id(job_id);
  vertex->entry()->set_job(job);
  vertex->entry()->set_state(WorkerJobEntry::BLOCKED);

  // Add edges for the new job.
  IDSet<job_id_t>::ConstIter iter = job->before_set_p()->begin();
  for (; iter != job->before_set_p()->end(); ++iter) {
    job_id_t before_job_id = *iter;
    // TODO(omidm): what if this is too small, controller do not support after
    // map and before set clean up for worker template!!!
    if (InFinishHintSet(before_job_id)) {
      continue;
    }
    WorkerJobVertex* before_job_vertex = NULL;
    // TODO(omidm): if the job is not in the job graph could we say that it was
    // done and removed. isn't controller assigning before set before job?!!!
    if (worker_job_graph_.HasVertex(before_job_id)) {
      // The job is already known.
      worker_job_graph_.GetVertex(before_job_id, &before_job_vertex);
    } else {
      if (IDMaker::SchedulerProducedJobID(before_job_id)) {
        // Local job is acknowledged locally.
        continue;
      }
      // The job is unknown.
      worker_job_graph_.AddVertex(before_job_id, new WorkerJobEntry());
      worker_job_graph_.GetVertex(before_job_id, &before_job_vertex);
      before_job_vertex->entry()->set_job_id(before_job_id);
      before_job_vertex->entry()->set_job(NULL);
      before_job_vertex->entry()->set_state(WorkerJobEntry::PENDING);
    }
    // If that job is not finished.
    if (before_job_vertex->entry()->get_state() != WorkerJobEntry::FINISH) {
      worker_job_graph_.AddEdge(before_job_vertex, vertex);
    }
  }

  // If the job has no dependency, it is ready.
  if (vertex->incoming_edges()->empty()) {
    vertex->entry()->set_state(WorkerJobEntry::READY);
    ResolveDataArray(job);
    int success_flag = worker_manager_->PushJob(job);
    vertex->entry()->set_job(NULL);
    assert(success_flag);
  }
  timer::StopTimer(timer::kJobGraph2);
}

void Worker::ClearAfterSet(WorkerJobVertex* vertex) {
  timer::StartTimer(timer::kClearAfterSet);
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);

  WorkerJobEdge::Map* outgoing_edges = vertex->outgoing_edges();
  // Deletion inside loop is dangerous.
  std::list<WorkerJobVertex*> deletion_list;
  for (WorkerJobEdge::Iter iter = outgoing_edges->begin();
       iter != outgoing_edges->end();
       ++iter) {
    WorkerJobVertex* after_job_vertex = (iter->second)->end_vertex();
    assert(after_job_vertex != NULL);
    deletion_list.push_back(after_job_vertex);
  }
  std::list<Job*> job_list;
  for (std::list<WorkerJobVertex*>::iterator iter = deletion_list.begin();
       iter != deletion_list.end();
       ++iter) {
    WorkerJobVertex* after_job_vertex = *iter;
    worker_job_graph_.RemoveEdge(vertex, after_job_vertex);
    if (after_job_vertex->incoming_edges()->empty()) {
      after_job_vertex->entry()->set_state(WorkerJobEntry::READY);
      assert(after_job_vertex->entry()->get_job() != NULL);
      ResolveDataArray(after_job_vertex->entry()->get_job());
      job_list.push_back(after_job_vertex->entry()->get_job());
      after_job_vertex->entry()->set_job(NULL);
    }
  }
  bool success_flag = worker_manager_->PushJobList(&job_list);
  assert(success_flag);
  timer::StopTimer(timer::kClearAfterSet);
}

void Worker::NotifyLocalJobDone(Job* job) {
  bool template_job = false;
  bool need_to_send_job_done = true;
  MegaJobDoneCommand *mega_job_done_comm = NULL;
  bool mark_stat = false;
  if (job->name().find("__MARK_STAT") != std::string::npos) {
    mark_stat = true;
  }
  {
    timer::StartTimer(timer::kJobGraph3);
    boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
    StatEndJob(1);

    job_id_t job_id = job->id().elem();
    data_map_.ReleaseAccess(job_id);
    job_id_t shadow_job_id = job->shadow_job_id();
    if (shadow_job_id != NIMBUS_KERNEL_JOB_ID) {
      template_job = true;
      ExecutionTemplate *et = job->execution_template();
      assert(et);
      JobList ready_jobs;
      if (et->MarkInnerJobDone(shadow_job_id, &ready_jobs, prepare_rewind_phase_, mark_stat, false)) { // NOLINT
        assert(ready_jobs.size() == 0);
        et->GenerateMegaJobDoneCommand(&mega_job_done_comm);
        std::map<template_id_t, ExecutionTemplate*>::iterator iter =
          active_execution_templates_.find(et->template_generation_id());
        assert(iter != active_execution_templates_.end());
        active_execution_templates_.erase(iter);
        if (et->pending_instantiate()) {
          template_id_t tgi = et->pending_template_generation_id();
          EventMap::iterator it = pending_events_.find(tgi);
          if (it != pending_events_.end()) {
            et->InstantiatePending(it->second,
                                   &ready_jobs);
            pending_events_.erase(it);
          } else {
            WorkerDataExchanger::EventList empty_pending_events;
            et->InstantiatePending(empty_pending_events,
                                   &ready_jobs);
          }
          assert(!et->pending_instantiate());
          StatAddJob(et->job_num());
          active_execution_templates_[tgi] = et;
        }
      }
      JobList::iterator iter = ready_jobs.begin();
      for (; iter != ready_jobs.end(); ++iter) {
        ResolveDataArray(*iter);
      }
      bool success_flag = worker_manager_->PushJobList(&ready_jobs);
      assert(success_flag);
    } else {
      // Job done for unknown job is not handled.
      if (!worker_job_graph_.HasVertex(job_id)) {
        // The job must be in the local job graph.
        assert(false);
        return;
      }

      // if it is a copy job, signal execution templates! -omidm
      if (IDMaker::SchedulerProducedJobID(job_id)) {
        if (active_execution_templates_.size() > 0) {
          JobList ready_jobs;
          {
            std::map<template_id_t, ExecutionTemplate*>::iterator iter =
              active_execution_templates_.begin();
            for (; iter != active_execution_templates_.end(); ++iter) {
              iter->second->NotifyJobDone(job_id, &ready_jobs, prepare_rewind_phase_, true);
            }
          }
          {
            JobList::iterator iter = ready_jobs.begin();
            for (; iter != ready_jobs.end(); ++iter) {
              ResolveDataArray(*iter);
            }
          }
          bool success_flag = worker_manager_->PushJobList(&ready_jobs);
          assert(success_flag);
        }
      }


      WorkerJobVertex* vertex = NULL;
      worker_job_graph_.GetVertex(job_id, &vertex);
      assert(vertex->incoming_edges()->empty());
      ClearAfterSet(vertex);
      delete vertex->entry();
      worker_job_graph_.RemoveVertex(job_id);
      if (!IDMaker::SchedulerProducedJobID(job_id)) {
        AddFinishHintSet(job_id);
      }
      // vertex->entry()->set_state(WorkerJobEntry::FINISH);
      // vertex->entry()->set_job(NULL);
    }

    // If in the prepare rewind phase, then there is no need to send the job
    // done command. because we do not need to make progress anymore. Note that
    // if you decided to send the job done, then make sure that it is sent
    // within the lock to make sure that the job done is sent before prepare
    // rewind command. For other case job done sending does not need to be
    // protected so for the sake of performance do it out of the locked
    // section. -omidm
    if (prepare_rewind_phase_) {
      need_to_send_job_done = false;
    }

    job_graph_cond_.notify_all();
  }

  if (need_to_send_job_done) {
    if (!template_job)
      SendJobDoneAndDeleteJob(job, mark_stat);
    else if (mega_job_done_comm != NULL) {
      client_->SendCommand(mega_job_done_comm);
      delete mega_job_done_comm;
    }
  }

  timer::StopTimer(timer::kJobGraph3);
}

void Worker::SendJobDoneAndDeleteJob(Job* job, bool mark_stat) {
  Parameter params;
  SaveDataJob *j = dynamic_cast<SaveDataJob*>(job); // NOLINT
  if (j != NULL) {
    SaveDataJobDoneCommand cm(j->id(), j->run_time(), j->wait_time(), j->max_alloc(),
                              ID<checkpoint_id_t>(j->checkpoint_id()), j->handle());
    client_->SendCommand(&cm);
  } else if ((!IDMaker::SchedulerProducedJobID(job->id().elem())) || (!job->sterile())) {
    JobDoneCommand cm(job->id(), job->run_time(), job->wait_time(), job->max_alloc(), false, mark_stat); // NOLINT
    client_->SendCommand(&cm);
  }

  delete job;
}

void Worker::NotifyJobDone(job_id_t job_id, bool final) {
  dbg(DBG_WORKER_FD,
      DBG_WORKER_FD_S"Job(#%d) is removed in the local job graph.\n", job_id);
  if (IDMaker::SchedulerProducedJobID(job_id)) {
    // Jobdone command for local job is not handled.
    return;
  }

  timer::StartTimer(timer::kJobGraph4);
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);

  if (final) {
    // Job done for unknown job is not handled.
    if (!worker_job_graph_.HasVertex(job_id)) {
      timer::StopTimer(timer::kJobGraph4);
      return;
    }
    WorkerJobVertex* vertex = NULL;
    worker_job_graph_.GetVertex(job_id, &vertex);
    assert(vertex->incoming_edges()->empty());
    assert(vertex->entry()->get_job() == NULL);
    if (vertex->entry()->get_state() != WorkerJobEntry::FINISH) {
      std::cout << "OMID: why waiting for controller's job done?!\n";
      ClearAfterSet(vertex);
    }
    delete vertex->entry();
    worker_job_graph_.RemoveVertex(job_id);
  } else {
    if (worker_job_graph_.HasVertex(job_id)) {
      WorkerJobVertex* vertex = NULL;
      worker_job_graph_.GetVertex(job_id, &vertex);
      assert(vertex->incoming_edges()->empty());
      assert(vertex->entry()->get_job() == NULL);
      vertex->entry()->set_state(WorkerJobEntry::FINISH);
      ClearAfterSet(vertex);
    } else {
      AddFinishHintSet(job_id);
    }
  }
  timer::StopTimer(timer::kJobGraph4);
}

void Worker::ProcessRCREvent(const WorkerDataExchanger::Event& e) {
  WorkerJobVertex* vertex = NULL;
  if (worker_job_graph_.HasVertex(e.receive_job_id_)) {
    worker_job_graph_.GetVertex(e.receive_job_id_, &vertex);
    switch (vertex->entry()->get_state()) {
      case WorkerJobEntry::PENDING: {
        // The job is already in the graph and not received.
        vertex->entry()->set_version(e.version_);
        vertex->entry()->set_ser_data(e.ser_data_);
        vertex->entry()->set_state(WorkerJobEntry::PENDING_DATA_RECEIVED);
        break;
      }
      case WorkerJobEntry::BLOCKED: {
        // The job is already in the graph and received.
        assert(vertex->entry()->get_job() != NULL);
        RemoteCopyReceiveJob* receive_job =
            dynamic_cast<RemoteCopyReceiveJob*>(vertex->entry()->get_job());  // NOLINT
        assert(receive_job != NULL);
        receive_job->set_data_version(e.version_);
        receive_job->set_serialized_data(e.ser_data_);

        // Remove the dumb edge and could be ready now.
        worker_job_graph_.RemoveEdge(DUMB_JOB_ID, e.receive_job_id_);
        if (vertex->incoming_edges()->empty()) {
          vertex->entry()->set_state(WorkerJobEntry::READY);
          ResolveDataArray(receive_job);
          int success_flag = worker_manager_->PushJob(receive_job);
          vertex->entry()->set_job(NULL);
          assert(success_flag);
        }
        break;
      }
      default:
        assert(false);
    }  // End switch.
  } else {
    // The job is not in the graph and not received.
    worker_job_graph_.AddVertex(e.receive_job_id_, new WorkerJobEntry());
    worker_job_graph_.GetVertex(e.receive_job_id_, &vertex);
    vertex->entry()->set_job_id(e.receive_job_id_);
    vertex->entry()->set_job(NULL);
    vertex->entry()->set_version(e.version_);
    vertex->entry()->set_ser_data(e.ser_data_);
    vertex->entry()->set_state(WorkerJobEntry::PENDING_DATA_RECEIVED);
  }
}

void Worker::ProcessMegaRCREvent(const WorkerDataExchanger::Event& e) {
  WorkerJobVertex* vertex = NULL;
  if (worker_job_graph_.HasVertex(e.mega_rcr_job_id_)) {
    worker_job_graph_.GetVertex(e.mega_rcr_job_id_, &vertex);
    switch (vertex->entry()->get_state()) {
      case WorkerJobEntry::PENDING:
      case WorkerJobEntry::PENDING_MEGA_DATA_RECEIVED: {
        // The job is already in the graph and not received.
        // TODO(omidm): fis the version passing for mega RCR!
        vertex->entry()->set_version(e.version_);
        vertex->entry()->set_ser_data(e.receive_job_id_, e.ser_data_);
        vertex->entry()->set_state(WorkerJobEntry::PENDING_MEGA_DATA_RECEIVED);
        break;
      }
      case WorkerJobEntry::BLOCKED: {
        // The job is already in the graph and received.
        assert(vertex->entry()->get_job() != NULL);
        MegaRCRJob* mega_rcr_job =
            dynamic_cast<MegaRCRJob*>(vertex->entry()->get_job());  // NOLINT
        assert(mega_rcr_job != NULL);
        // TODO(omidm): fis the version passing for mega RCR!
        // mega_rcr_job->set_data_version(e.version_);
        mega_rcr_job->set_serialized_data(e.receive_job_id_, e.ser_data_);

        if (mega_rcr_job->AllDataReceived()) {
          // Remove the dumb edge and could be ready now.
          worker_job_graph_.RemoveEdge(DUMB_JOB_ID, e.mega_rcr_job_id_);
          if (vertex->incoming_edges()->empty()) {
            vertex->entry()->set_state(WorkerJobEntry::READY);
            ResolveDataArray(mega_rcr_job);
            int success_flag = worker_manager_->PushJob(mega_rcr_job);
            vertex->entry()->set_job(NULL);
            assert(success_flag);
          }
        }
        break;
      }
      default:
        assert(false);
    }  // End switch.
  } else {
    // The job is not in the graph and not received.
    worker_job_graph_.AddVertex(e.mega_rcr_job_id_, new WorkerJobEntry());
    worker_job_graph_.GetVertex(e.mega_rcr_job_id_, &vertex);
    vertex->entry()->set_job_id(e.mega_rcr_job_id_);
    vertex->entry()->set_job(NULL);
    // TODO(omidm): fis the version passing for mega RCR!
    vertex->entry()->set_version(e.version_);
    vertex->entry()->set_ser_data(e.receive_job_id_, e.ser_data_);
    vertex->entry()->set_state(WorkerJobEntry::PENDING_MEGA_DATA_RECEIVED);
  }
}


void Worker::ProcessReceiveEvents(const WorkerDataExchanger::EventList& events) {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  JobList ready_jobs;
  WorkerDataExchanger::EventList::const_iterator iter;
  for (iter = events.begin(); iter != events.end(); ++iter) {
    template_id_t tgi = iter->template_generation_id_;
    if (tgi != NIMBUS_INVALID_TEMPLATE_ID) {
      std::map<template_id_t, ExecutionTemplate*>::iterator it =
        active_execution_templates_.find(tgi);
      if (it != active_execution_templates_.end()) {
        it->second->ProcessReceiveEvent(*iter, &ready_jobs, true);
      } else {
        pending_events_[tgi].push_back(*iter);
      }
    } else {
      if (iter->mega_rcr_job_id_ == NIMBUS_KERNEL_JOB_ID) {
        ProcessRCREvent(*iter);
      } else {
        ProcessMegaRCREvent(*iter);
      }
    }
  }

  JobList::iterator i = ready_jobs.begin();
  for (; i != ready_jobs.end(); ++i) {
    ResolveDataArray(*i);
  }
  bool success_flag = worker_manager_->PushJobList(&ready_jobs);
  assert(success_flag);
}


void Worker::AddFinishHintSet(const job_id_t job_id) {
  if (hint_set_.find(job_id) != hint_set_.end()) {
    return;
  }
  if (hint_set_.size() < max_hint_size_) {
    hint_set_.insert(job_id);
    hint_queue_.push_back(job_id);
  } else {
    hint_set_.erase(hint_queue_.front());
    hint_queue_.pop_front();
    hint_set_.insert(job_id);
    hint_queue_.push_back(job_id);
  }
}

bool Worker::InFinishHintSet(const job_id_t job_id) {
  return hint_set_.find(job_id) != hint_set_.end();
}


void Worker::ClearBlockedJobs() {
  boost::unique_lock<boost::recursive_mutex> lock(job_graph_mutex_);
  std::list<job_id_t> list_to_remove;
  typename WorkerJobVertex::Iter iter = worker_job_graph_.begin();
  for (; iter != worker_job_graph_.end(); ++iter) {
    WorkerJobEntry* job_entry = iter->second->entry();
    if (job_entry->get_state() != WorkerJobEntry::CONTROL &&
        job_entry->get_state() != WorkerJobEntry::READY) {
      list_to_remove.push_back(job_entry->get_job_id());
      if (job_entry->get_job()) {
        delete job_entry->get_job();
      }
      job_entry->set_job(NULL);
      delete job_entry;
    }
  }

  std::list<job_id_t>::iterator it = list_to_remove.begin();
  for (; it != list_to_remove.end(); ++it) {
    worker_job_graph_.RemoveVertex(*it);
  }
}

bool Worker::AllReadyJobsAreDone() {
  {
    typename WorkerJobVertex::Iter iter = worker_job_graph_.begin();
    for (; iter != worker_job_graph_.end(); ++iter) {
      WorkerJobEntry* job_entry = iter->second->entry();
      if (job_entry->get_state() != WorkerJobEntry::CONTROL) {
        return false;
      }
    }
  }
  {
    std::map<template_id_t, ExecutionTemplate*>::iterator iter =
      active_execution_templates_.begin();
    for (; iter != active_execution_templates_.end(); ++iter) {
      if (iter->second->ready_job_counter() != 0) {
        return false;
      }
    }
  }

  return true;
}

void Worker::StatAddJob(size_t num) {
  boost::unique_lock<boost::recursive_mutex> lock(stat_mutex_);
  // printf("add %d %d %d %d %d\n",
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
  assert((stat_idle_cores_ >= 0) && (num >= 0));
  stat_blocked_job_num_ += num;
  size_t diff = std::min(stat_idle_cores_, num);
  if (diff > 0) {
    stat_idle_cores_ -= diff;
    stat_blocked_cores_ += diff;
    // timer::StartTimer(timer::kSumCyclesBlock, diff);
    block_timer_.Start(diff);
  }
  // printf("#add %d %d %d %d %d\n",
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
}
void Worker::StatDispatchJob(size_t num) {
  boost::unique_lock<boost::recursive_mutex> lock(stat_mutex_);
  // printf("%d dis %d %d %d %d %d\n", num,
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
  assert(stat_blocked_job_num_ >= num);
  stat_blocked_job_num_ -= num;
  stat_ready_job_num_ += num;
  if (stat_blocked_cores_ > 0) {
    size_t release_cores = std::min(stat_blocked_cores_, num);
    if (release_cores > 0) {
      stat_blocked_cores_ -= release_cores;
      // timer::StopTimer(timer::kSumCyclesBlock, release_cores);
      block_timer_.Stop(release_cores);
      stat_busy_cores_ += release_cores;
      // timer::StartTimer(timer::kSumCyclesRun, release_cores);
      run_timer_.Start(release_cores);
    }
  }
  // printf("#dis %d %d %d %d %d\n",
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
}
void Worker::StatEndJob(size_t num) {
  boost::unique_lock<boost::recursive_mutex> lock(stat_mutex_);
  // printf("%d end %d %d %d %d %d\n", num,
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
  stat_ready_job_num_ -= num;
  size_t busy_cores =
    std::min(stat_ready_job_num_,
             static_cast<size_t>(WorkerManager::across_job_parallism));
  size_t blocked_cores =
    std::min(stat_blocked_job_num_,
             static_cast<size_t>(WorkerManager::across_job_parallism) - busy_cores);
  size_t idle_cores =
      WorkerManager::across_job_parallism - busy_cores - blocked_cores;
  if (busy_cores != stat_busy_cores_) {
    // timer::StopTimer(timer::kSumCyclesRun, stat_busy_cores_ - busy_cores);
    run_timer_.Stop(stat_busy_cores_ - busy_cores);
  }
  if (blocked_cores != stat_blocked_cores_) {
    // timer::StartTimer(timer::kSumCyclesBlock, blocked_cores - stat_blocked_cores_);
    block_timer_.Start(blocked_cores - stat_blocked_cores_);
  }
  stat_busy_cores_ = busy_cores;
  stat_blocked_cores_ = blocked_cores;
  stat_idle_cores_ = idle_cores;
  // printf("#end %d %d %d %d %d\n",
  //        stat_busy_cores_, stat_blocked_cores_, stat_idle_cores_,
  //        stat_blocked_job_num_, stat_ready_job_num_);
}

// The unit is in nano-second.
void Worker::GetTimerStat(int64_t* idle, int64_t* block, int64_t* run) {
  boost::unique_lock<boost::recursive_mutex> lock(stat_mutex_);
  static int64_t l_idle = 0, l_block = 0, l_run = 0;
  // int64_t c_block = timer::ReadTimer(timer::kSumCyclesBlock);
  int64_t c_block = block_timer_.Read();
  // int64_t c_run = timer::ReadTimer(timer::kSumCyclesRun);
  int64_t c_run = run_timer_.Read();
  // int64_t c_idle = timer::ReadTimer(timer::kSumCyclesTotal) - c_block - c_run;
  int64_t c_idle = total_timer_.Read() - c_block - c_run;
  *idle = c_idle - l_idle;
  *block = c_block - l_block;
  *run = c_run - l_run;
  l_idle = c_idle;
  l_block = c_block;
  l_run = c_run;
}

// The unit is in nano-second.
void Worker::PrintTimerStat() {
  boost::unique_lock<boost::recursive_mutex> lock(stat_mutex_);
  std::string file_name = int2string(id_) + "_main_timers.txt";
  static FILE* temp = fopen(file_name.c_str(), "w");
  static int64_t l_idle = 0, l_block = 0, l_run = 0, l_copy = 0, l_rcrx = 0, l_pexec = 0, l_dxl = 0, l_ivm = 0, l_cas = 0, l_j1 = 0, l_j2 = 0, l_j3 = 0, l_j4 = 0; // NOLINT
  int64_t c_block = block_timer_.Read();
  int64_t c_run = run_timer_.Read();
  int64_t c_idle = total_timer_.Read() - c_block - c_run;
  int64_t c_copy = timer::ReadTimerTypeSum(timer::kExecuteCopyJob);
  int64_t c_rcrx = timer::ReadTimerTypeSum(timer::kRCRCopy);
  int64_t c_pexec = timer::ReadTimerTypeSum(timer::kExecuteParentJob);
  int64_t c_dxl = timer::ReadTimerTypeSum(timer::kDataExchangerLock);
  int64_t c_ivm = timer::ReadTimerTypeSum(timer::kInvalidateMappings);
  int64_t c_cas = timer::ReadTimerTypeSum(timer::kClearAfterSet);
  int64_t c_j1 = timer::ReadTimerTypeSum(timer::kJobGraph1);
  int64_t c_j2 = timer::ReadTimerTypeSum(timer::kJobGraph2);
  int64_t c_j3 = timer::ReadTimerTypeSum(timer::kJobGraph3);
  int64_t c_j4 = timer::ReadTimerTypeSum(timer::kJobGraph4);
  int64_t idle = c_idle - l_idle;
  int64_t block = c_block - l_block;
  int64_t run = c_run - l_run;
  int64_t copy = c_copy - l_copy;
  int64_t rcrx = c_rcrx - l_rcrx;
  int64_t pexec = c_pexec - l_pexec;
  int64_t dxl = c_dxl - l_dxl;
  int64_t ivm = c_ivm - l_ivm;
  int64_t cas = c_cas - l_cas;
  int64_t j1 = c_j1 - l_j1;
  int64_t j2 = c_j2 - l_j2;
  int64_t j3 = c_j3 - l_j3;
  int64_t j4 = c_j4 - l_j4;
  l_idle = c_idle;
  l_block = c_block;
  l_run = c_run;
  l_copy = c_copy;
  l_rcrx = c_rcrx;
  l_pexec = c_pexec;
  l_dxl = c_dxl;
  l_ivm = c_ivm;
  l_cas = c_cas;
  l_j1 = c_j1;
  l_j2 = c_j2;
  l_j3 = c_j3;
  l_j4 = c_j4;
  fprintf(temp, "run_time: %.3f block_time: %.3f idle_time: %.3f parent_exec: %.3f dx_lock: %.3f copy_time: %.3f rcr_copy: %.3f inv_map: %.3f jg1: %.3f jg2: %.3f jg3: %.3f jg4: %.3f clear_as %.3f\n", // NOLINT
      static_cast<double>(run) / 1e9,
      static_cast<double>(block) / 1e9,
      static_cast<double>(idle) / 1e9,
      static_cast<double>(pexec) / 1e9,
      static_cast<double>(dxl) / 1e9,
      static_cast<double>(copy) / 1e9,
      static_cast<double>(rcrx) / 1e9,
      static_cast<double>(ivm) / 1e9,
      static_cast<double>(j1) / 1e9,
      static_cast<double>(j2) / 1e9,
      static_cast<double>(j3) / 1e9,
      static_cast<double>(j4) / 1e9,
      static_cast<double>(cas) / 1e9);
  fflush(temp);
#ifdef _RUN_STRAGGLER_SCENARIO
  static double start_time = Log::GetRawTime();
  if (id_ == 8) {
    static int phase = 0;
    if ((phase == 0) && ((Log::GetRawTime() - start_time) > 600)) {
      ++phase;
      // straggling ratio is set to 10x.
      system("../../ec2/create-straggler.sh 10");
    }
    if ((phase == 1) && ((Log::GetRawTime() - start_time) > 2100)) {
      ++phase;
      system("../../ec2/create-straggler.sh t");
    }
  }
  if (id_ == 1) {
    if ((Log::GetRawTime() - start_time) > 2100) {
      exit(0);
    }
  }
#endif
}


}  // namespace nimbus
