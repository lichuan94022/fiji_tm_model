/*********************************************************************
 * ZYZYX CONFIDENTIAL
 * __________________
 * 
 * 2017 Zyzyx Incorporated 
 * All Rights Reserved.
 * 
 * NOTICE:  All information contained herein is, and remains
 * the property of Zyzyx Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Zyzyx Incorporated and its suppliers 
 * and may be covered by U.S. and Foreign Patents, patents in process, 
 * and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Zyzyx Incorporated.
 *
 * File: $File$
 * Revision: $Revision$
 * Author: $Author$
 * Creation Date: $Date$
 *
 * Description:
 *
*********************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<queue>
#include<math.h>

#include "Scheduler.hpp"

//////////////////////////////////
// Main + Functions
//////////////////////////////////
int main(int argc,      // Number of strings in array argv  
          char *argv[],   // Array of command-line argument strings  
          char *envp[] )  // Array of environment variable strings  
{
//  logger::dv_debug("**********************************\n");
//  logger::dv_debug("Initializing knobs\n");
//  logger::dv_debug("**********************************\n");
//  sknobs_init(argc, argv);

  logger::dv_debug(DV_INFO, "**********************************\n");
  logger::dv_debug(DV_INFO, "Initializing TM Output Scheduler Model!\n");
  logger::dv_debug(DV_INFO, "**********************************\n");
  Scheduler tm_sched(argc, argv);

  logger::dv_debug(DV_INFO, "**********************************\n");
  logger::dv_debug(DV_INFO, "Running TM Output Scheduler Model!\n");
  logger::dv_debug(DV_INFO, "**********************************\n");

  if(tm_sched.run_scheduler()){
    logger::dv_debug(DV_INFO, "Exiting due to error\n");
  }
  else {
    logger::dv_debug(DV_INFO, "**********************************\n");
    logger::dv_debug(DV_INFO, "Completed TM Output Scheduler Model Run!\n");
    logger::dv_debug(DV_INFO, "****************************************\n");

    tm_sched.report_stats(); 
    tm_sched.post_run_checks(); 
  }

  logger::dv_debug(DV_INFO, "Completed TM Output Scheduler Model Run!\n");

}
