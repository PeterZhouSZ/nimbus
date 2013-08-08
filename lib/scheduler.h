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
  * Nimbus scheduler. 
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#ifndef NIMBUS_LIB_SCHEDULER_H_
#define NIMBUS_LIB_SCHEDULER_H_

#include <boost/thread.hpp>
#include <iostream> // NOLINT
#include <fstream> // NOLINT
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "lib/nimbus.h"
#include "lib/scheduler_server.h"
#include "lib/cluster.h"
#include "lib/parser.h"

namespace nimbus {
class Scheduler {
  public:
    explicit Scheduler(unsigned int listening_port);
    virtual ~Scheduler();

    void run();
    virtual void schedulerCoreProcessor() {}
    void loadClusterMap(std::string);
    void deleteWorker(Worker * worker);
    Worker * addWorker();
    Worker * getWorker(int workerId);

  private:
    void setupUserInterface();
    void setupWorkerInterface();
    void getUserCommand();
    void loadUserCommands();
    void loadWorkerCommands();

    boost::thread* user_interface_thread_;

    CmSet user_command_set_;
    CmSet worker_command_set_;

    Computer host_;
    uint16_t port_;
    uint64_t appId_;

    SchedulerServer* server_;

    // AppMap appMap;
    WorkerMap worker_map_;
    ClusterMap cluster_map_;
};

}  // namespace nimbus
#endif  // NIMBUS_LIB_SCHEDULER_H_