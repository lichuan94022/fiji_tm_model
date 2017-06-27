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
#ifndef __SHAPER_HPP__
#define __SHAPER_HPP__

#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<queue>
#include<string>
#include "logger.hpp"


//////////////////////////////////
// Class: Shaper
// 
// Description: 
//   Basic token bucket shaper
//     Tokens are equivalient to bytes.
//     Tokens are incremented at a period interval based on configured rate.
//     Tokens are decremented when pkt is sent.
//
//////////////////////////////////
class Shaper {
    bool sign; // 0=positive number, 1=negative number
    double cur_num_tokens;
    int clks_before_refresh;
    int refresh_interval;
    std::string name;

    double bytes_per_clk;
    double bytes_per_refresh;

  public:

    //////////////////////////////////////////
    // Function: init_shaper
    //  
    // Return Value: 1 = "initialization successful"
    //               0 = "initialization failed" 
    //
    // Arguments: name - name of shaper
    //            shaper_rate (in Gbps) - rate at which shaper allows transmission
    //            clk_period (in ns) - clock period of shaper
    //            shaper_refresh_interval (in clks) - time between token additions
    //
    // Description: 
    //   Initializes shaper by setting the number of bytes per refresh,
    //   refresh_interval, clks before_refresh, and cur_num_tokens
    //   Note: refresh refers to adding tokens to bucket (or shaper).
    //
    //////////////////////////////////////////
    bool init_shaper(std::string name, double shaper_rate, double clk_period, int shaper_refresh_interval);

    //////////////////////////////////////////
    // Function: init_shaper
    //  
    // Return Value: none
    //  
    // Description: 
    //   Should be called every "clk cycle" to maintain shaper credits and
    //   time to next token refresh (increment)
    //////////////////////////////////////////
    void update_shaper();

    //////////////////////////////////////////
    // Function: check_shaper
    //  
    // Return Value: 1 = "yes, okay to transmit"
    //               0 = "no, queue packet"
    //
    // Arguments: num_pkt_bytes (in bytes) - pkt bytes requesting transmission
    //            wait_clks (in clks) - time to wait before okay to transmit
    //                                - only valid when shaper has indicated "no"
    // Description: 
    //   Should be called every "clk cycle" to maintain shaper credits and
    //   time to next token refresh (increment)
    //////////////////////////////////////////
    bool check_shaper(int num_pkt_bytes, double & wait_clks); 

    //////////////////////////////////////////
    // Function: decr_shaper
    // 
    // Return Value: none
    //  
    // Arguments: num_pkt_bytes (in bytes) - pkt bytes requesting transmission
    //
    // Description: 
    //   Should be called when pkt which has been waiting is scheduled for
    //   transmission.
    //////////////////////////////////////////
    void decr_shaper(int num_pkt_bytes); 
  
};
 
#endif
