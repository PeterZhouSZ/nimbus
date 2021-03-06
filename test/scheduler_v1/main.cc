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
  * This file has the main function that launches Nimbus scheduler.
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#define DEBUG_MODE

#include <iostream> // NOLINT
#include "./scheduler_v1.h"
#include "shared/nimbus.h"
#include "shared/nimbus_types.h"
#include "shared/scheduler_command.h"
#include "shared/parser.h"

int main(int argc, char *argv[]) {
  nimbus::nimbus_initialize();

  SchedulerV1 * s = new SchedulerV1(NIMBUS_SCHEDULER_PORT);

  if (argc < 2) {
      dbg(DBG_SCHED, "Nothig provided for min initial number of workers, using default.\n");
  } else {
    std::string str(argv[1]);
    std::cout << str << std::endl;
    std::stringstream ss(str);
    size_t num;
    ss >> num;
    if (ss.fail()) {
      dbg(DBG_SCHED, "Invalid input for min initial number of workers, using default.\n");
    } else {
       s->set_min_worker_to_join(num);
      dbg(DBG_SCHED, "Set min initial number of workers to %d.\n", num);
    }
  }

  s->Run();
}

