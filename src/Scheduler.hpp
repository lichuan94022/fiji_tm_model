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

#ifndef __SCHEDULER_HPP__
#define __SCHEDULER_HPP__


#include<stdio.h>
#include<stdlib.h>
#include<deque>
#include<string>
#include<sstream>
#include<math.h>
#include <vector>

#include "Shaper.hpp"
#include "PktGen.hpp"
#include "OutputHandler.hpp"
#include "sknobs.h"

enum check_type_e {NEW_PKT_DELAYED=0, NO_NEW_PKT=1, NO_MORE_PKTS=2};
enum delay_type_e {PUSH_TO_WAIT_QUEUE=0, PUSH_TO_HEAP=1, PUSH_TO_BOTH=2};

///////////////////////////////////////////////////////////////////////
//  Class: Scheduler
//
//  Description: Modeling a possible new algorithm for traffic scheduling
//    Each packet is presented to its associated vlan and queue shaper.
//    If both shapers allow the pkt to pass, it will go to the output at once.
//    If vlan shaper passes, but queue shaper fails, pkt will wait based
//      on queue shaper criteria and vlan shaper will decrement as if pkt had
//      passed.
//    If queue shaper passes, but vlan shaper fails, pkt will wait based
//      on vlan shaper criteria and vlan shaper will decrement as if pkt had
///////////////////////////////////////////////////////////////////////
class Scheduler {
  public:

    // Knob Variables
    std::string knob_debug_level;
    double knob_clk_freq;
    double knob_clk_period;
    double **knob_qshaper_rate;
    double *knob_vshaper_rate;
    int **knob_qshaper_refresh_interval;
    int *knob_vshaper_refresh_interval;

    int knob_num_total_pkts;
    int *knob_num_queues;
    int knob_num_vlans;

    int knob_gather_bytes_per_clk;
    double knob_oport_rate;
    int knob_num_ofifo_entries;

    // Simulation Variables
    double total_time_in_ns;
    double clk_period;
    
    // Simulation Structural Variables
    PktGen pkt_gen;
    Shaper **queue_shaper;
    Shaper *vlan_shaper;
    std::deque <PktInfo> **pkt_wait_queue;
    std::priority_queue<PktInfo, std::vector<PktInfo> > pkt_wait_heap;
    OutputHandler output_handler;
    PktInfo new_pkt;

    // Statistical Variables
    double max_wait_time_in_clks;
    double **max_wait_time_in_clks_per_queue;

    double total_wait_time_in_ns;
    double **total_wait_time_in_ns_per_queue;

    int num_pkts_imm_send ;
    int **num_pkts_imm_send_per_queue;

    int num_bytes_imm_send ;
    int **num_bytes_imm_send_per_queue;

    int num_pkts_delayed_send ;
    int **num_pkts_delayed_send_per_queue;

    int num_bytes_delayed_send ;
    int **num_bytes_delayed_send_per_queue;

    double pcent_imm_send;
    double **pcent_imm_sends_per_queue;

    ////////////////////////////////////////////////////////////////////////////
    // Function: get_knobs
    //
    // Return Value: None
    //
    // Arguments: argc - number of arguments
    //            argv - vector of arguments
    //
    // Description: Read knob file and create and set knob variables
    ////////////////////////////////////////////////////////////////////////////
    void get_knobs(int, char*[]);

    ////////////////////////////////////////////////////////////////////////////
    // Function: check_for_delayed_pkt_ready
    //
    // Return Value: 0 = no simulation error
    //               1 = simulation error
    //
    // Arguments: check_type 
    //              NEW_PKT_DELAYED - checking when new pkt has been delayed this cycle
    //              NO_NEW_PKT - checking when no new pkt is available this cycle
    //              NO_MORE_PKTS - checking when no more new pkts are arriving
    //
    // Description: 
    //   If heap isn't empty,
    //     Check if top of heap send time > current clock time
    //       Update counters
    //       Decrement appropriate shapers
    //       Remove pkt from heap and wait queue
    //       Send to output_handler
    //       If another pkt is waiting, check shapers to calculate wait time and
    //        place on heap
    ////////////////////////////////////////////////////////////////////////////
    bool check_for_delayed_pkt_ready(check_type_e check_type); 

    ////////////////////////////////////////////////////////////////////////////
    // Function: run_scheduler
    //
    // Return Value: 0 = simulation error
    //               1 = simulation error
    // Arguments: None
    //
    // Description: 
    //   While not all the packets have been sent, there are still delayed packets
    //   or packets in the output handler,
    //     if the output handler is not full
    //       if a new packet needs to be generated,
    //          if associated wait_queue is > 0, 
    //            queue pkt in wait_queue
    //          else    
    //            check shapers
    //              if both pass, immediate send
    //              if one pass, delay with shaper wait time
    //              if no pass, delay with > shaper wait time
    //            if pkt delayed, check wait heap for delayed pkt that is ready to send
    //       else
    //         check wait heap for delayed pkt that is ready to send
    //           if pkt ready
    //             if associated wait queue has another pkt
    //               check shapers and determine wait time
    //     update shaper
    //     update output_handler
    //     update stats
    //          
    ////////////////////////////////////////////////////////////////////////////
    bool run_scheduler(); 

    ////////////////////////////////////////////////////////////////////////////
    // Function: queue_delay_pkt
    //
    // Return Value: None
    //
    // Arguments: pkt - pkt to queue
    //            delay_type  
    //                PUSH_TO_WAIT_QUEUE - Push only to wait queue (when queue>0)
    //                PUSH_TO_HEAP - Push only to heap (when queue head is popped
    //                                 and queue>0)
    //                PUSH_TO_BOTH - Push to wait queue and heap (when queue==0)
    //
    // Description: 
    //   Push pkt to wait queue and/or heap dependent on delay_type argument.   
    ////////////////////////////////////////////////////////////////////////////
    void queue_delay_pkt(PktInfo& pkt, delay_type_e delay_type);

    ////////////////////////////////////////////////////////////////////////////
    // Function: print_pkt_heap_top
    //
    // Return Value: none
    //
    // Arguments: none
    //  
    // Description: Print contents of top of pkt_heap
    ////////////////////////////////////////////////////////////////////////////
    void report_stats(); 

    ////////////////////////////////////////////////////////////////////////////
    // Function: post_run_checks
    //
    // Return Value: none
    //
    // Arguments: none
    //  
    // Description: Print out any remaining pkts in data structures (error behavior)
    ////////////////////////////////////////////////////////////////////////////
    void post_run_checks();

    ////////////////////////////////////////////////////////////////////////////
    // Function: print_pkt_wait_queue
    //
    // Return Value: none
    //
    // Arguments: vlan 
    //            qnum
    //  
    // Description: Print contents of vlan/queue pkt_wait_queue
    ////////////////////////////////////////////////////////////////////////////
    void print_pkt_wait_queue(int vlan, int qnum);

    ////////////////////////////////////////////////////////////////////////////
    // Function: print_pkt_heap_top
    //
    // Return Value: none
    //
    // Arguments: none
    //  
    // Description: Print contents of top of pkt_heap
    ////////////////////////////////////////////////////////////////////////////
    void print_pkt_heap_top();

    Scheduler(int, char*[]); 
    ~Scheduler();

};

#endif
