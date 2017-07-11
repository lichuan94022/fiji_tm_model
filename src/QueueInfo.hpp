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
#ifndef __QUEUE_INFO_HPP__
#define __QUEUE_INFO_HPP__

#include "logger.hpp"

//////////////////////////////////
// Class: QueueInfo
// 
// Description:
//    Each queue has the following properties:
//      priority - transmission importance level relative to other queues
//      weight - applicable for same priority and when within above max and below min rate
//             - amount of bandwidth relative to other queues
//      max_rate - maximum rate
//      min_rate - minimum rate
//      above_max - above maximum rate
//      below_min - below minimum rate
//////////////////////////////////
class QueueInfo {


public:

  // Queue Variables
  int priority;
  int weight;

  // Queue Variables
  double max_rate;
  double min_rate;

  // Queue Status Variables
  int above_max;
  int below_min;
  double cur_rate;


  ////////////////////////////////////////////////////////////////////////
  // Function: print
  ////////////////////////////////////////////////////////////////////////
  void print() const {
    logger::dv_debug(DV_DEBUG1, "QueueInfo: priority=%0d, weight=%0d, min_rate=%2f, max_rate=%2f, cur_rate=%2f, below_min=%0d, above_max=%0d\n",
	                      priority, weight, min_rate, max_rate, cur_rate, below_min, above_max);
  }

};

#endif
