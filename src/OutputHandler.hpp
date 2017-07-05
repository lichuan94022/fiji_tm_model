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

#ifndef __OUTPUT_HANDLER_HPP__
#define __OUTPUT_HANDLER_HPP__

#include<stdio.h>
#include<stdlib.h>
#include<vector>
#include<string>
#include<sstream>

#include "PktInfo.hpp"
#include "sknobs.h"

class OutputHandler {
  public:

    // Knob Variables
    int knob_gather_bytes_per_clk;
    double knob_oport_rate;
    int knob_num_ofifo_entries;
    int knob_num_vlans;
    int *knob_num_queues;
    int knob_num_total_pkts;

    // Simulation Variables
    double clk_period;
    double obytes_per_clk;

    // Status Variables
    double cur_oport_rate;
    int full_time;
    double total_output_bytes;
    double total_output_time;
    double **total_output_bytes_per_queue;
    double **last_clk_count_per_queue;
    double **first_clk_count_per_queue;
    int **init_clk_count_per_queue;
    int num_rcvd_pkts;

    // Structural Variables
    std::vector <PktInfo> output_fifo;
    std::vector <PktInfo> **output_fifo_per_queue;

    // Operational Functions
    void init(int num_vlans, int* num_queues, int num_total_pkts, double new_clk_period, double oport_rate, int num_ofifo_entries, int gather_bytes_per_clk);
    void add_pkt(PktInfo &new_pkt); 
    void update_output_handler(double total_time); 

    // Status Functions
    bool is_full(); 
    bool is_empty(); 

    // Print Functions
    void print_fifo(double total_time); 
    void print_fifo_per_queue(int vlan, int qnum, double total_time); 
    void print_stats(); 

    ~OutputHandler();
};

#endif
