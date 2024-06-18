/*
 * Copyright 2024 HP Inc.
 * All rights reserved.
 *
 * This software and associated documentation (if any) is furnished
 * under a license and may only be used or copied in accordance
 * with the terms of the license.
 * 
 * Authors: Muhammad Anisur Rahman, Ananth Kunchakara
 * Date: May 04, 2024 
 * Version: Development version drop 1
 * License: HP authorizes free modify/copy/commercial/personal  use to Linux users community
 * 
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#include "fu-hpi-cfu-struct.h"

#define FU_TYPE_HPI_CFU_DEVICE (fu_hpi_cfu_device_get_type())
G_DECLARE_DERIVABLE_TYPE(FuHpiCfuDevice, fu_hpi_cfu_device, FU, HPI_CFU_DEVICE, FuHidDevice)

struct _FuHpiCfuDeviceClass {
        FuUsbDeviceClass parent_class;
};