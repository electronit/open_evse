// -*- C++ -*-
/*
 * Open EVSE Firmware
 *
 * Copyright (c) 2011-2012 Sam C. Lin <lincomatic@gmail.com> and Chris Howell <chris1howell@msn.com>
 * Maintainers: SCL/CH

 * This file is part of Open EVSE.

 * Open EVSE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.

 * Open EVSE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with Open EVSE; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Time.h>

#if defined(ARDUINO) && (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h" // shouldn't need this but arduino sometimes messes up and puts inside an #ifdef
#endif // ARDUINO

// includes are here because arduino seems to really only include files in the main pde file.
#include "Config.h"

#if defined(RGBLCD) || defined(I2CLCD)
// Using LiquidTWI2 for both types of I2C LCD's
// see http://blog.lincomatic.com/?p=956 for installation instructions
#include <Wire.h>
#include <LiquidTWI2.h>
#endif // RGBLCD || I2CLCD

#ifdef NHDLCD
#include <LCDi2cNHD.h>
#endif // NHDLCD

// serial port command line
#ifdef SERIALCLI
#include "CLI.h"
#endif

// 1 button menu (comment to disable)
#ifdef BTN_MENU
#include "ButtonMenu.h"
#endif

// Onboard display
#include "OnboardDisplay.h"

// OpenEVSE
#include "Open_EVSE.h"



