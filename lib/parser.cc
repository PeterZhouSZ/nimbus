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
  * Parser for Nimbus scheduler protocol. 
  *
  * Author: Omid Mashayekhi <omidm@stanford.edu>
  */

#include "lib/parser.h"



void parseCommand(const std::string& str, const CmSet& cms,
                  std::string& cm, std::vector<int>& args) {
  cm.clear();
  args.clear();
  std::set<std::string>::iterator it;
  for (it = cms.begin(); it != cms.end(); ++it) {
    if (str.find(*it) == 0) {
      cm = *it;
      int arg;
      std::stringstream ss;
      ss << str.substr(it->length(), std::string::npos);
      while (true) {
        ss >> arg;
        if (ss.fail()) {
          break;
        }
        args.push_back(arg);
      }
      break;
    }
  }
  if (cm == "") {
    std::cout << "wrong command! try again." << std::endl;
  }
}


int parseCommandFile(const std::string& fname, CmSet& cs) {
  return 0;
}

void parseCommandFromString(const std::string input,
                            std::string& command,
                            std::vector<std::string>& parameters) {
  unsigned int pos = 0;
  unsigned int count = 0;
  for (;;) {
    unsigned int next = input.find(' ', pos);
    if (next == std::string::npos) {break;}
    std::string token(input.substr(pos, next + 1));
    if (count == 0) {
      command = token;
    } else {
      parameters.push_back(token);
    }
    pos = next + 1;
    count++;
  }
}



