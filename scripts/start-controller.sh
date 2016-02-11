#!/usr/bin/env bash

#
#  Copyright 2013 Stanford University.
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
# 
#  - Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 
#  - Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the
#    distribution.
# 
#  - Neither the name of the copyright holders nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#  FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
#  THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
#  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
#  OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Starts Nimbus controller on the machine this script is executed on.

# Author: Omid Mashayekhi <omidm@stanford.edu>

# **************************
# Text Reset
RCol='\x1B[0m'    
# Regular           
Bla='\x1B[0;30m';
Red='\x1B[0;31m';
Gre='\x1B[0;32m';
Yel='\x1B[0;33m';
Blu='\x1B[0;34m';
Pur='\x1B[0;35m';
Cya='\x1B[0;36m';
Whi='\x1B[0;37m';
# **************************

if [ -z "${NIMBUS_HOME}" ]; then
  export NIMBUS_HOME="$(cd "`dirname "$0"`"/..; pwd)"
fi

if [ -z "${DBG}" ]; then
  export DBG="error"
fi

if [ -z "${TTIMER}" ]; then
  export TTIMER="l1"
fi

CONTROLLER_DIR="${NIMBUS_HOME}/nodes/nimbus_controller/"
CONTROLLER_BIN="nimbus_controller"
LOG_DIR="${NIMBUS_HOME}/logs"
ARGS="$@"

if [[ ${ARGS} = *--help* ]] || [[ ${ARGS} = *-h* ]]; then
  echo -e "${Blu}Launches the nimbus controller on the machine this script is executed on."
  echo -e "Usage: ./scripts/start-controller.sh <controller options>"
  echo -e "           - worker number is set to 1 by default."
  echo -e "           - listening port is set to 5900 by default.\n"
  cd ${CONTROLLER_DIR}; "./${CONTROLLER_BIN}" -h 2>&1
  echo -e "${RCol}"
  exit 1
fi

echo -e "${Blu}NIMBUS_HOME  ... \"${NIMBUS_HOME}\"${RCol}"
echo -e "${Blu}DBG  ........... \"${DBG}\"${RCol}"
echo -e "${Blu}TTIMER  ........ \"${TTIMER}\"${RCol}"

worker_num_given=false
for arg in ${ARGS}; do
  if [ "--worker_num" == "${arg}" ] || [ "-w" == "${arg}" ]; then
    worker_num_given=true
    break
  fi
done
if [ "${worker_num_given}" == "false" ]; then
  ARGS="-w 1 "${ARGS}
fi

port_num_given=false
for arg in ${ARGS}; do
  if [ "--port" == "${arg}" ] || [ "-p" == "${arg}" ]; then
    port_num_given=true
    break
  fi
done
if [ "${port_num_given}" == "false" ]; then
  ARGS="-p 5900 "${ARGS}
fi

if [ -e "${LOG_DIR}/controller" ]; then
  echo -e "${Yel}WARNING: found old controller log folder (only one older log is kept!)${RCol}"
  rm -rf "${LOG_DIR}/controller-old"
  mv "${LOG_DIR}/controller" "${LOG_DIR}/controller-old"
fi
mkdir -p "${LOG_DIR}/controller"

cd ${CONTROLLER_DIR}; "./${CONTROLLER_BIN}" ${ARGS} 1>"${LOG_DIR}/controller/stdout" 2>"${LOG_DIR}/controller/stderr" &
echo -e "${Gre}Launched controller with arguments \"${ARGS}\"; find stdout/stderr at: ${LOG_DIR}/controller${RCol}"

