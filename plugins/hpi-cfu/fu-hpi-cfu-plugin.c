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

#include "config.h"

#include "fu-hpi-cfu-device.h"
#include "fu-hpi-cfu-plugin.h"

struct _FuHpiCfuPlugin {
	FuPlugin parent_instance;
};

G_DEFINE_TYPE(FuHpiCfuPlugin, fu_hpi_cfu_plugin, FU_TYPE_PLUGIN)

static void
fu_hpi_cfu_plugin_init(FuHpiCfuPlugin *self)
{
}

static void
fu_hpi_cfu_plugin_constructed(GObject *obj)
{
    FuPlugin *plugin = FU_PLUGIN(obj);
    FuContext *ctx = fu_plugin_get_context(plugin);
    fu_context_add_quirk_key(ctx, "HpiCfuVersionGetReport");
    fu_context_add_quirk_key(ctx, "HpiCfuOfferSetReport");
    fu_context_add_quirk_key(ctx, "HpiCfuOfferGetReport");
    fu_context_add_quirk_key(ctx, "HpiCfuContentSetReport");
    fu_context_add_quirk_key(ctx, "HpiCfuContentGetReport");
    fu_plugin_add_device_gtype(plugin, FU_TYPE_HPI_CFU_DEVICE);
}

static void
fu_hpi_cfu_plugin_class_init(FuHpiCfuPluginClass *klass)
{
    FuPluginClass *plugin_class = FU_PLUGIN_CLASS(klass);
    plugin_class->constructed = fu_hpi_cfu_plugin_constructed;
}
