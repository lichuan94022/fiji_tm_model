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

#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <stdarg.h>
#include <stdio.h>

typedef enum {DV_INFO=0, DV_DEBUG1=1, DV_DEBUG2=2, DV_DEBUG3=3} debug_level_e;


class logger {
public:

  static int unsigned m_clk_count;
  static int unsigned m_debug_level;

  static int dv_set_debug_level(debug_level_e debug_level) {
    m_debug_level = debug_level; 
  }

  static int dv_debug(debug_level_e debug_level, const char* fmt, ... ) {
    if (debug_level <= m_debug_level) {
      va_list args;
      va_start(args, fmt);
      printf("%08d: ", m_clk_count);
      vprintf(fmt, args);
      va_end(args);
    }
  }  
};

#endif
