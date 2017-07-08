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
#ifndef __PKT_GEN_HPP__
#define __PKT_GEN_HPP__

#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<sstream>
#include<queue>

#include "PktInfo.hpp"
#include "OutputHandler.hpp"
#include "sknobs.h"

//////////////////////////////////
// Class: PktGen
// 
// Description: 
//     Generates Pkts given configured Input rate
//
//////////////////////////////////
class PktGen {

  // Knob Variables
  int *knob_num_queues;
  int knob_num_vlans;
  int **knob_min_pkt_size;
  int **knob_max_pkt_size;
  double knob_oport_rate;
  double **knob_iqueue_rate;
  int knob_max_wait_queue_size;
  int knob_max_wait_queue_bytes;

  // Status Variables
  double **cur_iqueue_rate;
  int *cur_qnum;
  int cur_vnum;
  int cur_pkt_id;

  // Pointer to Output Handler
  OutputHandler *output_handler;

  public:

    // Statistical Tracking Variables
    double total_bytes;

    int num_pkts_sent;
    int **num_pkts_sent_per_queue;

    int num_bytes_sent;
    int **num_bytes_sent_per_queue;

    // Pointer to Scheduler Wait Queue
    std::vector <PktInfo> **m_scheduler_pkt_wait_queue;
    int **m_scheduler_pkt_wait_queue_bytes;

    //////////////////////////////////////////////////////////////////////
    // Function: init 
    //
    // Return Value: None
    //
    // Arguments: num_vlans - number of vlans
    //            num_queues - number of queues (per vlan)
    //            wait_queue - pointer to scheduler wait_queue
    //            wait_queue_bytes - pointer to scheduler wait_queue (# bytes in queue)
    //            ref_output_handler - pointer to output_handler
    //
    // Description: 
    //   Allocate dynamic arrays, and set pointers to wait queue and output_handler
    /////////////////////////////////////////////////////////////////////////
    //void init(int num_vlans, int* num_queues, std::vector <PktInfo> **wait_queue, OutputHandler *ref_output_handler);
    void init(int num_vlans, int* num_queues, std::vector <PktInfo> **wait_queue, int **wait_queue_bytes, OutputHandler *ref_output_handler);
//    void init(int num_vlans, int* num_queues, std::vector <PktInfo> **wait_queue, OutputHandler *ref_output_handler);


    //////////////////////////////////////////////////////////////////////
    // Function: need_new_pkt 
    //
    // Return Value: 1 = valid packet
    //               0 = no valid packet
    //
    // Arguments: new_pkt - new packet info
    //            total_time - needed to calculate input rate
    //
    // Description: 
    //   If output handler isn't full and not above input rate, create a new pkt.
    //   Also, keep track of input rates for every queue.
    /////////////////////////////////////////////////////////////////////////
    bool need_new_pkt(PktInfo &new_pkt, double total_time); 

    //////////////////////////////////////////////////////////////////////
    // Function: select_new_pkt 
    //
    // Return Value: 1 = valid packet
    //               0 = no valid packet
    //
    // Arguments: new_pkt - new packet info
    //
    // Description: 
    //   If current input rate is less than configured input rate and no wait queue
    //   exists for the current vlan and current qnum, generate a new pkt and select
    //   a new vlan and new queue.
    /////////////////////////////////////////////////////////////////////////
    bool select_new_pkt(PktInfo &);

    //////////////////////////////////////////////////////////////////////
    // Function: report_stats 
    //
    // Description: 
    //   Report on number of pkts/bytes sent as well as expected vs actual rates.
    /////////////////////////////////////////////////////////////////////////
    void report_stats(double);

    PktGen();
    ~PktGen();

};
 
#endif
