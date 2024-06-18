---
title: Plugin: HPI-CFU - HPI Component Firmware Update
---

## Introduction

HPI-CFU is a plugin implemented based on cfu protocol from Microsoft to make it easy to install firmware on HID devices.

See <https://docs.microsoft.com/en-us/windows-hardware/drivers/cfu/cfu-specification> for more
details.

This plugin supports the following protocol ID:

* `com.microsoft.cfu`

## Implementation Notes

HPI-CFU is implemented based on cfu protocol and it requires downloading the metadata from LVFS to perform firmware update.
