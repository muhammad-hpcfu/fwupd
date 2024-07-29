/*
 * Copyright 2024 Owner Name <owner.name@hp.com>
 *
 */

#pragma once

#include <fwupdplugin.h>


#define FU_TYPE_HPI_CFU_DEVICE (fu_hpi_cfu_device_get_type())
G_DECLARE_DERIVABLE_TYPE(FuHpiCfuDevice, fu_hpi_cfu_device, FU, HPI_CFU_DEVICE, FuUsbDevice)

struct _FuHpiCfuDeviceClass {
	FuUsbDeviceClass parent_class;
};
