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

#[derive(New, ValidateBytes, Parse)]
struct FuStructHpiCfu {
    signature: u8 == 0xDE,
    address: u16le,
}

#[derive(ToString)]
enum FuHpiCfuStatus {
    Unknown,
    Failed,
}

typedef struct {
        guint8 op;
        guint8 id;
        guint8 ct;
} FuCfuDeviceMap;

#[derive(ToString)]
enum FuHpiCfuState {
    StartEntireTransaction = 0x01,
    StartEntireTransactionAccepted = 0x02,
    StartOfferList = 0x03,
    StartOfferListAccepted = 0x04,
    UpdateOffer = 0x05,
    UpdateOfferAccepted = 0x06,
    UpdateContent = 0x07,
    UpdateSuccess = 0x08,
    UpdateOfferRejected = 0x09,
    OfferSkip = 0x0A,
    UpdateMoreOffers = 0x0B,
    EndOfferList = 0x0C,
    EndOfferListAccepted = 0x0D,
    UpdateAllOfferRejected = 0x0E,
    UpdateStop = 0x0F,
    StateError = 0x10,
    CheckUpdateContent = 0x11,
    NotifyOnReady = 0x12,
    WaitForReadyNotification = 0x13,
    VerifyCheckSwapPendingBySendingOfferListAgain = 0x14,
    VerifyCheckSwapPendingOfferListAccepted = 0x15,
    VerifyCheckSwapPendingSendOffer = 0x16,
    VerifyCheckSwapPendingOfferAccepted = 0x17,
    VerifyCheckSwapPendingSendUpdateEndOfferList = 0x18,
    VerifyCheckSwapPendingUpdateEndOfferListAccepted = 0x19,
    UpdateVerifyError = 0x1A,
    WaitDeviceReboot = 0x1B,
    FuHpiCfuDeviceMap version_get_report,
    FuHpiCfuDeviceMap offer_set_report,
    FuHpiCfuDeviceMap offer_get_report,
    FuHpiCfuDeviceMap content_set_report,
    FuHpiCfuDeviceMap content_get_report,
}

#[derive(ToString)]
#[repr(u8)]
enum FuHpiCfuOfferInfoCode {
    StartEntireTransaction = 0x01,
    StartOfferList = 0x01,
    EndOfferList = 0x02,
}

