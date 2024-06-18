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

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "fu-hpi-cfu-device.h"
#include "fu-hpi-cfu-struct.h"

/* SOME USB PROTOCOL DEFINES */
#define GET_REPORT   0x01
#define SET_REPORT   0x09
#define GET_REPORT_REQUEST_TYPE 0xA1
#define SET_REPORT_REQUEST_TYPE 0x21
#define IN_REPORT_TYPE  0x0100
#define OUT_REPORT_TYPE  0x0200
#define FEATURE_REPORT_TYPE  0x0300
#define VERSION_REPORT_ID    0x20
#define CFU_INTERFACE_INDEX 0x01
#define REPORT_LENGTH 64
#define OFFER_REPORT_ID 0x01
#define START_ENTIRE_TRANSACTION_REPORT_ID 0x25
#define OFFER_INFO_START_OFFER_LIST_ID  0x25
#define OFFER_INFO_END_OFFER_LIST_ID  0x25
#define FIRMWARE_UPDATE_OFFER_ID 0x20
#define FWUPDATE_CONTENT_REPORT_ID 0x20



/* DEVICE RESPONSE STATUS */
#define FIRMWARE_UPDATE_OFFER_SKIPPED       0x00
#define FIRMWARE_UPDATE_OFFER_ACCEPTED      0x01
#define FIRMWARE_UPDATE_OFFER_REJECTED      0x02
#define FIRMWARE_UPDATE_OFFER_COMMAND_READY     0x04


//SOME DEFAULTS
#define DEFAULT_VERSION_FEATURE_USAGE VERSION_REPORT_ID
#define DEFAULT_CONTENT_OUTPUT_USAGE FWUPDATE_CONTENT_REPORT_ID
#define DEFAULT_CONTENT_RESPONSE_INPUT_USAGE FWUPDATE_CONTENT_REPORT_ID
#define DEFAULT_OFFER_OUTPUT_USAGE FIRMWARE_UPDATE_OFFER_ID
#define DEFAULT_OFFER_RESPONSE_INPUT_USAGE FIRMWARE_UPDATE_OFFER_ID


#define FU_HPI_CFU_DEVICE_POLL_INTERVAL 1000



//****************************************************************************
//                                  DEFINES
//****************************************************************************
// NOTE - defines should match CFU Protocol Spec definitions
#define CFU_OFFER_METADATA_INFO_CMD                        (0xFF)
#define CFU_SPECIAL_OFFER_CMD                              (0xFE)
#define CFU_SPECIAL_OFFER_GET_STATUS                       (0x03)
#define CFU_SPECIAL_OFFER_NONCE                            (0x02)
#define CFU_SPECIAL_OFFER_NOTIFY_ON_READY                  (0x01)
#define CFW_UPDATE_PACKET_MAX_LENGTH                       (sizeof(FuHpiCfuFwUpdateContentCommand))
#define FIRMWARE_OFFER_REJECT_BANK                         (0x04)
#define FIRMWARE_OFFER_REJECT_PLATFORM                     (0x05)
#define FIRMWARE_OFFER_REJECT_MILESTONE                    (0x06)
#define FIRMWARE_OFFER_REJECT_INV_PCOL_REV                 (0x07)
#define FIRMWARE_OFFER_REJECT_VARIANT                      (0x08)
#define FIRMWARE_OFFER_REJECT_INV_MCU                      (0x01)
#define FIRMWARE_OFFER_REJECT_MISMATCH                     (0x03)
#define FIRMWARE_OFFER_REJECT_OLD_FW                       (0x00)
#define FIRMWARE_OFFER_TOKEN_DRIVER                        (0xA0)
#define FIRMWARE_OFFER_TOKEN_SPEEDFLASHER                  (0xB0)
#define FIRMWARE_UPDATE_CMD_NOT_SUPPORTED                  (0xFF)
#define FIRMWARE_UPDATE_FLAG_FIRST_BLOCK                   (0x80)
#define FIRMWARE_UPDATE_FLAG_LAST_BLOCK                    (0x40)
#define FIRMWARE_UPDATE_FLAG_VERIFY                        (0x08)
#define FIRMWARE_UPDATE_OFFER_ACCEPT                       (0x01)
#define FIRMWARE_UPDATE_OFFER_BUSY                         (0x03)
#define FIRMWARE_UPDATE_OFFER_COMMAND_READY                (0x04)
#define FIRMWARE_UPDATE_OFFER_REJECT                       (0x02)
#define FIRMWARE_UPDATE_OFFER_SKIP                         (0x00)
#define FIRMWARE_UPDATE_OFFER_SWAP_PENDING                 (0x02)
#define FIRMWARE_UPDATE_STATUS_ERROR_COMPLETE              (0x03)
#define FIRMWARE_UPDATE_STATUS_ERROR_CRC                   (0x05)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID               (0x0B)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID_ADDR          (0x09)
#define FIRMWARE_UPDATE_STATUS_ERROR_NO_OFFER              (0x0A)
#define FIRMWARE_UPDATE_STATUS_ERROR_PENDING               (0x08)
#define FIRMWARE_UPDATE_STATUS_ERROR_PREPARE               (0x01)
#define FIRMWARE_UPDATE_STATUS_ERROR_SIGNATURE             (0x06)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERIFY                (0x04)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERSION               (0x07)
#define FIRMWARE_UPDATE_STATUS_ERROR_WRITE                 (0x02)
#define FIRMWARE_UPDATE_STATUS_SUCCESS                     (0x00)
#define OFFER_INFO_END_OFFER_LIST                          (0x02)
#define OFFER_INFO_START_ENTIRE_TRANSACTION                (0x00)
#define OFFER_INFO_START_OFFER_LIST                        (0x01)
#define REPORT_ID_VERSIONS_FEATURE                         (0x20)

//FILE * fp;
//FILE * logfilefp;
const gchar *fw_version_payload;
const gchar *fw_version_device;
int device_reset = 0;


unsigned char buffer_OFFER_INFO_START_ENTIRE_TRANSACTION[]={0x25,0x00,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
unsigned char buffer_OFFER_INFO_START_OFFER_LIST[]={0x25,0x01,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,};
unsigned char buffer_OFFER_INFO_END_OFFER_LIST[]={0x25,0x02,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};


#define FU_HPI_CFU_DEVICE_TIMEOUT 0 /* ms */
#define FU_HPI_CFU_DEVICE_FLAG_SEND_OFFER_INFO (1 << 0)

typedef struct {
    guint8 op;
    guint8 id;
    guint8 ct;
} FuHpiCfuDeviceMap;

/* This structure is used to read the data packet header inside the payload file */
typedef struct {
    gint32 offset;
    gchar length;
} __attribute__((packed))  FuHpiCfuPayloadHeader;

/* This structure is used to send the Firmware Update Content command */
typedef struct _FuHpiCfuFwUpdateContentCommand {
    gchar report_id;
    gint8 flags;
    gint8 length;
    gint16 sequence_number;
    gint32 address;
    gint8 data[52];
} __attribute__((packed)) FuHpiCfuFwUpdateContentCommand;

/* This structure is used as cache for payload data that has more than 52 bytes per packet */
typedef struct _FuHpiCfuPayloadRead {
    unsigned char buf[256];
    gint8 count;
    gint8 from_index_to_read;
} FuHpiCfuPayloadRead;

typedef struct {
    guint8 iface_number;
    FuHpiCfuState state;
    guint8 force_version;
    guint8 force_reset;
    FuHpiCfuPayloadRead pr;
    guint16 version_feature_usage;
    guint16 content_output_usage;
    guint16 content_response_input_usage;
    guint16 offer_output_usage;
    guint16 offer_response_input_usage;
    //COMPONENT_VERSION_AND_PROPERTY Components[7]; //Need to implement the logic for multiple offers
    gint32 sequence_number;
    gint32 currentaddress;
    gint32 bytes_sent;
    gint32 payload_file_size;
    gint32 bytes_remaining;
    gint32 last_packet_sent;
    gboolean claimed_interface;
    gint32 burst_acksize;
    //guint16 version;
} FuHpiCfuDevicePrivate;

typedef union _FuHpiCfuComponentVersion {
    struct {
        gint8 variant;
        gint16 minor_version;
        gint8 major_version;
    } __attribute__((packed)) header;
} __attribute__((packed)) FuHpiCfuComponentVersion;

typedef struct {
    gchar report_id;

    struct {
        gint8 segment_number;
        gint8 reserved0 : 6;
        gint8 force_immediate_reset : 1;
        gint8 force_ignore_version : 1;
        gint8 component_id;
        gint8 token;
    } __attribute__((packed)) FuHpiCfuComponentInfo;
    
    FuHpiCfuComponentVersion version;
    
    gint32 vendor_specific;
    
    struct {
        gint8 protocol_version : 4;
        gint8 reserved0 : 4;
        gint8 reserved1;
        gint16 vendor_specific;
    } __attribute__((packed)) FuHpiCfuMiscProtocolVersion;
} __attribute__((packed)) FuHpiCfuFwUpdateOfferCmdPkt;

G_DEFINE_TYPE_WITH_PRIVATE(FuHpiCfuDevice, fu_hpi_cfu_device, FU_TYPE_HID_DEVICE)
#define GET_PRIVATE(o) (fu_hpi_cfu_device_get_instance_private(o))



const gchar*
fu_hpi_response_str(gint8 ch)
{
    gchar *reason;
    
    static char buf[128];
    strcpy(buf,"Unknown response");

    switch (ch) {
        case FIRMWARE_UPDATE_OFFER_SKIPPED:
            strcpy(buf, "OFFER_SKIPPED");
            break;
        
        case FIRMWARE_UPDATE_OFFER_ACCEPTED:
            strcpy(buf, "OFFER_ACCEPTED");
            break;
        
        case FIRMWARE_UPDATE_OFFER_REJECTED:
            strcpy(buf, "OFFER_REJECTED");
            break;
        
        case FIRMWARE_UPDATE_OFFER_BUSY:
            strcpy(buf, "DEVICE_BUSY");
            break;
        
        case FIRMWARE_UPDATE_OFFER_COMMAND_READY:
            strcpy(buf, "COMMAND_READY");
            break;
        
        default:
            strcpy(buf, "UNKNOWN");
            break;
    }
    return (char*) &buf[0];
}


const gchar*
fu_hpi_cfu_reject_reason(gint8 selection)
{
    static char buf[128];
    strcpy(buf,"UNKNOWN_REJECT_REASON");
    gchar *reason;

    switch (selection) {
        case FIRMWARE_OFFER_REJECT_OLD_FW:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_OLD_FW");
            break;

        case FIRMWARE_OFFER_REJECT_INV_MCU:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_INV_MCU");
            break;

        case FIRMWARE_UPDATE_OFFER_SWAP_PENDING:
            strcpy(buf,"FIRMWARE_UPDATE_OFFER_SWAP_PENDING");
            break;

        case FIRMWARE_OFFER_REJECT_MISMATCH:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_MISMATCH");
            break;

        case FIRMWARE_OFFER_REJECT_BANK:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_BANK");
            break;

        case FIRMWARE_OFFER_REJECT_PLATFORM:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_PLATFORM");
            break;

        case FIRMWARE_OFFER_REJECT_MILESTONE:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_MILESTONE");
            break;

        case FIRMWARE_OFFER_REJECT_INV_PCOL_REV:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_INV_PCOL_REV");
            break;

        case FIRMWARE_OFFER_REJECT_VARIANT:
            strcpy(buf,"FIRMWARE_OFFER_REJECT_VARIANT");
            break;
    
        default:
            break;
    }
    return (char *) &buf[0];
}

static gboolean
fu_hpi_cfu_start_entire_transaction(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) start_entire_trans_request = NULL;
    
    start_entire_trans_request = g_bytes_new(buffer_OFFER_INFO_START_ENTIRE_TRANSACTION, sizeof(buffer_OFFER_INFO_START_ENTIRE_TRANSACTION));
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_start_entire_transaction sending:", start_entire_trans_request);

    if (!g_usb_device_control_transfer(usb_device,
                                           G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                           G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                           G_USB_DEVICE_RECIPIENT_DEVICE,
                                           SET_REPORT,
                                           OUT_REPORT_TYPE|START_ENTIRE_TRANSACTION_REPORT_ID,
                                           0,
                                           (unsigned char *) buffer_OFFER_INFO_START_ENTIRE_TRANSACTION,
                                           sizeof(buffer_OFFER_INFO_START_ENTIRE_TRANSACTION),
                                           NULL,
                                           FU_HPI_CFU_DEVICE_TIMEOUT,
                                           NULL,
                                           error)) {
	g_warning("failed send_start_entire_transaction with error: %s", error_local->message);
        return FALSE;
    }
    
    /* success */
    return TRUE; 
}


static gboolean
fu_hpi_cfu_start_entire_transaction_accepted(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    gsize actual_length = 0;
    guint8 buf[128] = {0};
    g_autoptr(GBytes) start_entire_trans_response = NULL;
    
    if (!g_usb_device_interrupt_transfer(usb_device,
                                            0x81,
                                            &buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed start_entire_transaction_accepted with error: %s", error_local->message);
	return FALSE;
    }
    
    g_debug("fu_hpi_cfu_start_entire_transaction_accepted: total bytes received: %d", actual_length);

    start_entire_trans_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "bytes received", start_entire_trans_response);
    
    /* success */
    if(buf[13]==0x01) {
        return TRUE;
    }
    
    return FALSE;
}


static gboolean
fu_hpi_cfu_send_start_offer_list(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) start_offer_list_request = NULL;

    start_offer_list_request = g_bytes_new(buffer_OFFER_INFO_START_OFFER_LIST, sizeof(buffer_OFFER_INFO_START_OFFER_LIST));
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_start_offer_list: sending", start_offer_list_request);

    if (!g_usb_device_control_transfer(usb_device,
                                            G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                            G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                            G_USB_DEVICE_RECIPIENT_DEVICE,
                                            SET_REPORT,
                                            OUT_REPORT_TYPE|OFFER_INFO_START_OFFER_LIST_ID,
                                            0,
                                            (unsigned char *) buffer_OFFER_INFO_START_OFFER_LIST,
                                            sizeof(buffer_OFFER_INFO_START_OFFER_LIST),
                                            NULL,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed send_offer_list with error: %s", error_local->message);
	return FALSE;
    }
    
    /* success */
    return TRUE;	
}

static gint8
fu_hpi_cfu_send_offer_list_accepted(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    gsize actual_length = 0;
    guint8 buf[128] = {0};
    g_autoptr(GBytes) send_offer_list_response = NULL;
    
    if (!g_usb_device_interrupt_transfer(usb_device,
                                            0x81,
                                            &buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed fu_hpi_cfu_device_send_offer_list_accepted with error: %s", error_local->message);
	return FALSE;
    }

    g_debug("fu_hpi_cfu_send_offer_list_accepted: total bytes received: %d", actual_length);
    send_offer_list_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_offer_list_accepted: bytes received", send_offer_list_response);
    
    /* success */
    if(buf[13] == 0x01) {
        g_debug("fu_hpi_cfu_device_send_offer_list_accepted success.");
    }
    else {
        if(buf[13] == 0x02) {
            g_set_error(error,
                    FWUPD_ERROR,
                    FWUPD_ERROR_INTERNAL,
                    "failed fu_hpi_cfu_device_send_offer_list_accepted with reason: %s",
                    fu_hpi_cfu_reject_reason(buf[9]));
        }
        else {
            g_set_error(error,
                    FWUPD_ERROR,
                    FWUPD_ERROR_INTERNAL,
                    "failed fu_hpi_cfu_device_send_offer_list_accepted with reason: %s",
                    fu_hpi_cfu_reject_reason(buf[9]));
        }
    }
    return buf[13];
}


static gint8
fu_hpi_cfu_send_offer_update_command(FuHpiCfuDevice *self, GError **error, FuFirmware *fw_offer)
{
    g_autoptr(GBytes) blob_offer = NULL;
    gsize size = 0;
    const guint8 *data = 0;
    FuHpiCfuFwUpdateOfferCmdPkt offer_command_packet;
    
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) offer_command_request = NULL;
    
    blob_offer = fu_firmware_get_bytes(fw_offer, NULL);
    data = g_bytes_get_data(blob_offer, &size);
    gchar *fw_offer_cmd = (gchar *) &offer_command_packet;
    memcpy(fw_offer_cmd + 1 , data, size);
    
    offer_command_packet.FuHpiCfuComponentInfo.force_immediate_reset = 1; //hardcoded for now (Update now)
    //offer_command_packet.FuHpiCfuComponentInfo.force_immediate_reset = 0; //hardcoded for now (Update on disconnect)
    offer_command_packet.FuHpiCfuComponentInfo.force_ignore_version = 1; //hardcoded for now (Force update version)
    offer_command_packet.report_id = OFFER_INFO_START_OFFER_LIST_ID;

    offer_command_request = g_bytes_new(fw_offer_cmd, size);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_offer_update_command sending:", offer_command_request);

    fw_version_payload = g_strdup_printf("%02X.%02X.%02X.%02X", fw_offer_cmd[8], fw_offer_cmd[7], fw_offer_cmd[6], fw_offer_cmd[5]);
    
    if (!g_usb_device_control_transfer(usb_device,
                                            G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                            G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                            G_USB_DEVICE_RECIPIENT_DEVICE,
                                            SET_REPORT,
                                            OUT_REPORT_TYPE|FIRMWARE_UPDATE_OFFER_ID,
                                            0,
                                            (unsigned char *) fw_offer_cmd,
                                            size + 1,
                                            NULL,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
	    g_warning("failed fu_hpi_cfu_send_offer_update_command with error: %s", error_local->message);
        return FALSE;
    }
    
    /* success */
    return 0;
}


static gint8
fu_hpi_cfu_firmware_update_offer_accepted(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    gsize actual_length = 0;
    guint8 buf[128] = {0};
    g_autoptr(GBytes) offer_response = NULL;
    
    if (!g_usb_device_interrupt_transfer(usb_device,
                                            0x81,
                                            &buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed fu_hpi_cfu_firmware_update_offer_accepted with error: %s", error_local->message);
	return FALSE;
    }
    
    g_debug("fu_hpi_cfu_firmware_update_offer_accepted: total bytes received: %d", actual_length);
    offer_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_firmware_update_offer_accepted: bytes received", offer_response);

    /* success */
    if(buf[13] == 0x01) {
        g_debug("fu_hpi_cfu_firmware_update_offer_accepted: success.");
    }
    else {
        if(buf[13] == 0x02) {
            g_warning("fu_hpi_cfu_firmware_update_offer_accepted: Not acceptance reason: %s", fu_hpi_cfu_reject_reason(buf[9]));
        }
        else {
            g_warning("fu_hpi_cfu_firmware_update_offer_accepted: Not acceptance reason: %s buf[13] is not a REJECT.", fu_hpi_cfu_reject_reason(buf[9]));
        }
    }

    return buf[13];
}


static gboolean
fu_hpi_cfu_send_firmware_update_content(FuHpiCfuDevicePrivate *priv, FuHpiCfuDevice *self, GError **error, FuFirmware *fw_payload)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) header_buf = NULL;
    g_autoptr(GBytes) payload_buf = NULL;
    g_autoptr(GBytes) untransmitted_buf = NULL;
    g_autoptr(GBytes) fw_content_command = NULL;

    
    g_autoptr(GBytes) blob_payload = NULL;
    gsize payload_file_size = 0;
    const guint8 *data = 0;
    
    FuHpiCfuPayloadHeader ph;
    FuHpiCfuFwUpdateContentCommand fcc;
    
    static unsigned char payload_data[256];
    memset(&payload_data[0], 0 , 256);
    memset(&ph, 0, sizeof(FuHpiCfuPayloadHeader));
    memset(&fcc, 0, sizeof(FuHpiCfuFwUpdateContentCommand));
    
    gint8 readlen;
    gint8 sendlen;
    gint8 existingbytecount;
    
    blob_payload = fu_firmware_get_bytes(fw_payload, NULL);
    data = g_bytes_get_data(blob_payload, &payload_file_size);
    
    static int curfilepos = 0;
    data = data + curfilepos;
    
    int curfilefilepos = 0;
    int header_len = 0;
    
    //g_debug("curfilepos:%d\n", curfilepos);
    
    if(priv->pr.count > 0) {
        if(priv->pr.count < 52) {
            if(curfilefilepos >= payload_file_size) {
                sendlen = priv->pr.count;
                
                memcpy((char*) &fcc.data[0], (char*) &priv->pr.buf[priv->pr.from_index_to_read], sendlen);
                priv->pr.count = 0;
                priv->pr.from_index_to_read = 0;
            }
            else {
                existingbytecount = priv->pr.count;
                memcpy((char*) &fcc.data[0], (char*) &priv->pr.buf[priv->pr.from_index_to_read], existingbytecount);

                //now read a new header
                header_len = sizeof(FuHpiCfuPayloadHeader);
                memcpy(&ph, data, sizeof(ph));

                g_debug("fu_hpi_cfu_send_firmware_update_content header size: %d", header_len);
                header_buf = g_bytes_new(&ph, header_len);
                fu_dump_bytes(G_LOG_DOMAIN, "header", header_buf);
                
                data = data + header_len;
                curfilefilepos = curfilefilepos + header_len;
                readlen = (int) ph.length;
                g_debug("fu_hpi_cfu_send_firmware_update_content: bytes payload read: %d", readlen);
                memcpy(&priv->pr.buf, data, readlen);
                data = data + readlen;
                curfilefilepos = curfilefilepos + readlen;

                priv->pr.count = readlen;
                priv->pr.from_index_to_read = 0;
                int fillsize = 52 - existingbytecount;

                if(readlen <= fillsize) {
                    sendlen = readlen + existingbytecount;
                    memcpy((char*) &fcc.data[existingbytecount], (char*) &priv->pr.buf[0], readlen);
                    priv->pr.from_index_to_read += readlen;
                    priv->pr.count -= readlen;
                }
                else {
                    sendlen = 52;
                    memcpy((char*) &fcc.data[existingbytecount], (char*) &priv->pr.buf[0], fillsize);
                    priv->pr.from_index_to_read += fillsize;
                    priv->pr.count -= fillsize;
                    
                    g_debug("fu_hpi_cfu_send_firmware_update_content: %d bytes of untransmitted data remain in cache", priv->pr.count);
                    //untransmitted_buf = g_bytes_new(priv->pr.buf, priv->pr.count);
                    //fu_dump_bytes(G_LOG_DOMAIN, "untransmitted bytes", untransmitted_buf);
                }
            }//<52
        } //if(pr.count<52)
        else { //cache has more than or equal to 52 bytes
            sendlen = 52;
            memcpy((char*) &fcc.data[0], (char*) &priv->pr.buf[priv->pr.from_index_to_read], sendlen);
            priv->pr.from_index_to_read = priv->pr.from_index_to_read + 52;
            priv->pr.count = priv->pr.count - 52;

            if(priv->pr.count) {

                g_debug("fu_hpi_cfu_send_firmware_update_content: %d bytes of untransmitted data remain in cache", priv->pr.count);
                //untransmitted_buf = g_bytes_new(priv->pr.buf, priv->pr.count);
                //fu_dump_bytes(G_LOG_DOMAIN, "untransmitted bytes", untransmitted_buf);
                /*for(int i = 0; i < priv->pr.count; i++) {
                    g_debug("%02X ", priv->pr.buf[priv->pr.from_index_to_read+i]);
                }*/
            }
        }

        //now check if no data in the buffer and end of file reached
        if(priv->pr.count == 0) {
            priv->last_packet_sent = (curfilefilepos >= payload_file_size) ? 1: 0;
            g_debug("fu_hpi_cfu_send_firmware_update_content: priv->last_packet_sent: %d", priv->last_packet_sent);
        }
    }
    else { //if no bytes in the pr
        existingbytecount = 0;

        //now read a new header
        header_len = sizeof(FuHpiCfuPayloadHeader);
        memcpy(&ph, data, sizeof(ph));

        g_debug("fu_hpi_cfu_send_firmware_update_content: Read payload header size: %d", header_len);
        
        header_buf = g_bytes_new(&ph, header_len);
        fu_dump_bytes(G_LOG_DOMAIN, "header", header_buf);
        
        g_debug("fu_hpi_cfu_send_firmware_update_content: offset: %08x, length: 0x%x", ph.offset,(unsigned char ) ph.length);

        data = data + header_len;
        curfilepos = curfilepos + header_len;
        
        readlen = (int) ph.length;
        g_debug("fu_hpi_cfu_send_firmware_update_content: bytes payload read: %d", readlen);

        memcpy(&priv->pr.buf, data, readlen);
        
        payload_buf = g_bytes_new(priv->pr.buf, readlen);
        fu_dump_bytes(G_LOG_DOMAIN, "payload data", payload_buf);
        
        data = data + readlen;
        curfilepos = curfilepos + readlen;

        priv->pr.count = readlen;
        priv->pr.from_index_to_read = 0;
        int fillsize = 52 - existingbytecount;

        if(readlen <= fillsize) {
            sendlen = readlen + existingbytecount;
            
            for(int i = 0; i < readlen; i++)
                fcc.data[i] = priv->pr.buf[i];
                
            priv->pr.from_index_to_read += readlen;
            priv->pr.count -= readlen;
        }
        else {
            sendlen = 52;
    
            for(int i = 0; i < readlen; i++)
                fcc.data[i] = priv->pr.buf[i];
                
            priv->pr.from_index_to_read += fillsize;
            priv->pr.count -= fillsize;
        }

        if(priv->pr.count) {

            g_debug("fu_hpi_cfu_send_firmware_update_content: %d bytes of untransmitted data remain in the cache", priv->pr.count);
            
            /*for(int i = 0; i < priv->pr.count; i++) {
                g_debug("%02X ", (unsigned char ) priv->pr.buf[priv->pr.from_index_to_read+i]);
            }*/
        }
        
        //now check if no data in the buffer and end of file reached
        if(priv->pr.count == 0) {
            priv->last_packet_sent = (curfilepos >= payload_file_size) ? 1: 0;
            g_debug("fu_hpi_cfu_send_firmware_update_content: priv->last_packet_sent=%d", priv->last_packet_sent);
        }
    }
    
    fcc.report_id = FWUPDATE_CONTENT_REPORT_ID;
    
    if(priv->sequence_number == 0)
        fcc.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
    else
        fcc.flags = (priv->last_packet_sent) ? FIRMWARE_UPDATE_FLAG_LAST_BLOCK : 0;
        
    fcc.length = sendlen;
    fcc.sequence_number = ++priv->sequence_number;
    fcc.address = priv->currentaddress;

    priv->currentaddress += sendlen;
    priv->bytes_sent += sendlen;
    priv->bytes_remaining = priv->payload_file_size - (priv->bytes_sent + header_len) - 5;

    g_debug("fu_hpi_cfu_send_firmware_update_content: priv->sequence_number: %d", priv->sequence_number);
    g_debug("fu_hpi_cfu_send_firmware_update_content: priv->bytes_sent: %d", priv->bytes_sent);
    g_debug("fu_hpi_cfu_send_firmware_update_content: priv->bytes_remaining: %d", priv->bytes_remaining);

    if(priv->last_packet_sent) {
        fcc.flags = FIRMWARE_UPDATE_FLAG_LAST_BLOCK;
    }
        
    g_debug("================== CONTENT DATA ==================");
    g_debug("fcc.report_id=%X",fcc.report_id);
    g_debug("fcc.flags=%X", (unsigned char) fcc.flags);
    g_debug("fcc.length=%d",fcc.length);
    g_debug("fcc.sequence_number=%02x",(unsigned short int) fcc.sequence_number);
    g_debug("fcc.address=%08X",fcc.address);
    
    g_debug("content pkt size=%d",sizeof(FuHpiCfuFwUpdateContentCommand));
    
    fw_content_command = g_bytes_new(&fcc, sizeof(FuHpiCfuFwUpdateContentCommand));
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_firmware_update_content: bytes sent", fw_content_command);

    g_debug("================== CONTENT DATA END==================");
    
    if(!g_usb_device_control_transfer(usb_device,
                                        G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                        G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                        G_USB_DEVICE_RECIPIENT_DEVICE,
                                        SET_REPORT,
                                        OUT_REPORT_TYPE|FWUPDATE_CONTENT_REPORT_ID,
                                        0,
                                        (unsigned char *) &fcc,
                                        sizeof(FuHpiCfuFwUpdateContentCommand),
                                        NULL,
                                        FU_HPI_CFU_DEVICE_TIMEOUT,
                                        NULL,
                                        &error_local)) {
        g_warning("failed fu_hpi_cfu_firmware_update_content command with error: %s", error_local->message);
	return FALSE;
    }

    return TRUE;
}

static gint8
fu_hpi_cfu_read_content_ack(FuHpiCfuDevicePrivate *priv, FuHpiCfuDevice *self, int *lastpacket, int *report_id, int *reason, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) read_ack_response = NULL;
    gsize actual_length = 0;
    guint8 buf[128];
    *report_id = 0;
    
    if (!g_usb_device_interrupt_transfer(usb_device,
                                            0x81,
                                            &buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed fu_hpi_cfu_read_content_ack with error: %s", error_local->message);
	return FALSE;
    }
    
    g_debug("fu_hpi_cfu_read_content_ack: bytes_received: %d", actual_length);
    read_ack_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_read_content_ack: bytes received", read_ack_response);
    
    *report_id = buf[0];
    /* success */
    if(buf[0] == FWUPDATE_CONTENT_REPORT_ID) {
        g_debug("fu_hpi_cfu_read_content_ack: buf[9]= %02X, buf[13]= %02x, Response= %s",(unsigned char)buf[9],(unsigned char)buf[13],fu_hpi_response_str((gint8)buf[13]));
        
        if(buf[13] == 0x01) {
            if(priv->last_packet_sent == 1)
                *lastpacket = 1;
            
            g_debug("read_content_ack:: last_packet_sent: 1");
            
            return  (int) buf[13];
        }
        return (int)buf[13];
    }
    else {
        g_debug("read_content_ack: buffer[5]: %02x, Response:%s",(unsigned char)buf[5], fu_hpi_response_str(buf[5]));
        
        if(buf[5] == 0x00) {
            g_debug("read_content_ack: 1");
            
            if(priv->last_packet_sent == 1)
                *lastpacket = 1;
    
            g_debug("read_content_ack:last_packet_sent: %d", *lastpacket);
            return (int) buf[5];
        }
        return (int) buf[5];
    }
    return -1;
}

static gboolean
fu_hpi_cfu_send_end_offer_list(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) end_offer_request = NULL;

    g_debug("fu_hpi_cfu_send_end_offer_list sending:");
    end_offer_request = g_bytes_new(buffer_OFFER_INFO_END_OFFER_LIST, sizeof(buffer_OFFER_INFO_END_OFFER_LIST));
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_start_entire_transaction sending:", end_offer_request);
    
    
    if(!g_usb_device_control_transfer(usb_device,
                                        G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                        G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                        G_USB_DEVICE_RECIPIENT_DEVICE,
                                        SET_REPORT,
                                        OUT_REPORT_TYPE|OFFER_INFO_END_OFFER_LIST_ID,
                                        0,
                                        (unsigned char *) buffer_OFFER_INFO_END_OFFER_LIST,
                                        sizeof(buffer_OFFER_INFO_END_OFFER_LIST),
                                        NULL,
                                        FU_HPI_CFU_DEVICE_TIMEOUT,
                                        NULL,
                                        &error_local)) {
        g_warning("failed fu_hpi_cfu_send_end_offer_list with error: %s", error_local->message);
        return FALSE;
    }

    /* success */
    return TRUE;
}

static gint8
fu_hpi_cfu_end_offer_list_accepted(FuHpiCfuDevice *self, GError **error)
{
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    gsize actual_length = 0;
    guint8 buf[128] = {0};
    g_autoptr(GBytes) end_offer_response = NULL;
    
    if (!g_usb_device_interrupt_transfer(usb_device,
                                            0x81,
                                            &buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            &error_local)) {
        g_warning("failed fu_hpi_cfu_end_offer_list_accepted with error: %s", error_local->message);
        return -1;
    }
    
    g_debug("fu_hpi_cfu_end_offer_list_accepted: bytes_received=%d", actual_length);
    end_offer_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_end_offer_list_accepted: bytes received", end_offer_response); 
    
    g_debug("END_OFFER_LIST_accepted: Identify Type = buffer[4]= %02x",(unsigned char )buf[4]);
    g_debug("END_OFFER_LIST_accepted: Reject Reason= buffer[9]= %02x, this value is meaningful when buffer[13]=2",(unsigned char )buf[9]);
    g_debug("END_OFFER_LIST_accepted: Reply Status = buffer[13]= %02x, Response:%s",(unsigned char )buf[13],fu_hpi_response_str((gint8)buf[13]));
    
    /* success */
    if(buf[13] == 0x01) {
        g_debug("END_OFFER_LIST_accepted:: buffer[13] = 1");
    }
    else {
        if(buf[13] == 0x02) {
	    g_warning("fu_hpi_cfu_firmware_update_offer_accepted: Not acceptance with reason: %s", fu_hpi_cfu_reject_reason(buf[9]));
        }
        else {
	    g_warning("fu_hpi_cfu_firmware_update_offer_accepted: Not acceptance with reason: %s but buf[13] is not REJECT", fu_hpi_cfu_reject_reason(buf[9]));
        }
    }

    return buf[13];
}

static gint8
fu_hpi_cfu_firmware_update_offer_rejected(gint8 reply)
{
    gint8 ret = 0;

    if(reply == FU_HPI_CFU_STATE_UPDATE_OFFER_REJECTED)
        ret = 1;
    
    return ret;
}

static gboolean
fu_hpi_cfu_update_firmware_state_machine(FuHpiCfuDevice *self, GError **error, FuProgress *progress, FuFirmware *fw_offer, FuFirmware *fw_payload)
{
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    gint8 reply;
    gint32 wait_for_burst_ack = 0;
    int status = 0;
    int count = 0;
    int report_id = 0;
    int reason = 0;
    int lastpacket = 0;
    int break_loop = 0;
    
    g_autoptr(GBytes) blob_payload = NULL;
    const guint8 *data;
    gsize payload_file_size = 0;
    
    blob_payload = fu_firmware_get_bytes(fw_payload, NULL);
    data = g_bytes_get_data(blob_payload, &payload_file_size);
    g_debug("payload_file_size: %d\n", payload_file_size);
    priv->payload_file_size = payload_file_size;

    fu_progress_set_id(progress, G_STRLOC);
    fu_progress_set_percentage(progress, 0);
    
    while(1) {
        switch(priv->state) {

            case FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION:
                g_debug("CFU STATE: FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION");

                if (!fu_hpi_cfu_start_entire_transaction(self, error))
                    priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
                    
                priv->state = FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION_ACCEPTED;
                break;
                
            case FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION_ACCEPTED:
                g_debug("priv->burst_acksize: %d", priv->burst_acksize);
                
                if ( !fu_hpi_cfu_start_entire_transaction_accepted(self, error))
                    priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
                    
                priv->state = FU_HPI_CFU_STATE_START_OFFER_LIST;
                break;
            
            case FU_HPI_CFU_STATE_START_OFFER_LIST:
                g_debug("CFU STATE: FU_HPI_CFU_STATE_START_OFFER_LIST");

                if ( !fu_hpi_cfu_send_start_offer_list(self, error))
                    priv->state = FU_HPI_CFU_STATE_STATE_ERROR;

                priv->state = FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED;
                break;
                
            case FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED:
                g_debug("CFU STATE: FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED");
                
                if ( fu_hpi_cfu_send_offer_list_accepted(self, error) >= 0)
                    priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
                else 
                    priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
                break;
            
            case FU_HPI_CFU_STATE_UPDATE_OFFER:
                g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_OFFER");

                if( fu_hpi_cfu_send_offer_update_command(self, error, fw_offer) >= 0 )
                    priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED;
                else
                    priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
                break;

            case FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED:
                g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED");
                
                reply = fu_hpi_cfu_firmware_update_offer_accepted(self, error);
                
                if(reply == FIRMWARE_UPDATE_OFFER_ACCEPT) {
                    g_debug("CFU STATE: CFU_FIRMWARE_UPDATE_OFFER_ACCEPT_3: reply=%x: OFFER_ACCEPTED", reply);
                    //cfu_obj->current_offer->offer_in_progress = 1;
                    //cfu_obj->current_offer->offer_rejected = 0;
                    //cfu_obj->current_offer->offer_skipped = 0;
                    priv->sequence_number = 0;
                    priv->currentaddress = 0;
                    priv->last_packet_sent = 0;
                    priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                }
                else {
                    //cfu_obj->current_offer->offer_in_progress = 0;
                    //cfu_obj->current_offer->offer_skipped = 0;
                    //cfu_obj->current_offer->offer_skipped = 0;

                    if (reply == FIRMWARE_UPDATE_OFFER_SKIP) {
                        //cfu_obj->current_offer->offer_skipped = 1;
                        g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply: %x, OFFER_SKIPPED", reply);
                        priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                    }
                    else if(reply == FIRMWARE_UPDATE_OFFER_REJECT) {
                        //cfu_obj->current_offer->offer_rejected=1;
                        g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply: %x, OFFER_REJECTED", reply);
                        priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                    }
                    else if(reply == FIRMWARE_UPDATE_OFFER_BUSY) {
                        //cfu_obj->current_offer->offer_skipped = 1;
                        g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply: %x, OFFER_BUSY", reply);
                        priv->state = FU_HPI_CFU_STATE_NOTIFY_ON_READY;
                    }
                    else {
                        //send a new offer from the list
                        priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                    }
                }
                break;
                
            case FU_HPI_CFU_STATE_UPDATE_CONTENT:

                reply = fu_hpi_cfu_send_firmware_update_content(priv, self, error, fw_payload);
                priv->state = FU_HPI_CFU_STATE_CHECK_UPDATE_CONTENT;
                fu_progress_set_percentage_full(progress, priv->bytes_sent, priv->payload_file_size);	
		break;
                
                if(reply == FALSE) {
                    g_debug("ERROR SENDING PAYLOAD COMMAND for sequence number:%d", priv->sequence_number);
                    priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
                    break;
                }
                
            case FU_HPI_CFU_STATE_CHECK_UPDATE_CONTENT:

                lastpacket = 0;
                wait_for_burst_ack = 0;

                if(priv->last_packet_sent) {
                    g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: last_packet_sent");
                    status = fu_hpi_cfu_read_content_ack(priv, self, &lastpacket, &report_id, &reason, error);
                }
                else {
                    
                    switch(priv->burst_acksize) {
                        
                        case 1:
                            if( (priv->sequence_number) % 16 == 0 ) {
                                status = fu_hpi_cfu_read_content_ack(priv, self, &lastpacket, &report_id, &reason, error);
                            }
                            else {
                                priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                                wait_for_burst_ack = 1;
                            }
                            break;
                            
                        case 2:
                            if( (priv->sequence_number) % 32 == 0 ) {
                                status = fu_hpi_cfu_read_content_ack(priv, self, &lastpacket, &report_id, &reason, error);
                            }
                            else {
                                priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                                wait_for_burst_ack = 1;
                            }
                            break;
                            
                        case 3:
                            if( (priv->sequence_number) % 64 == 0 ) {
                                status = fu_hpi_cfu_read_content_ack(priv, self, &lastpacket, &report_id, &reason, error);
                            }
                            else {
                                priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                                wait_for_burst_ack = 1;
                            }
                            break;

                        default:
                            status = fu_hpi_cfu_read_content_ack(priv, self, &lastpacket, &report_id, &reason, error);
                            break;
                    }
                }
                
                if(wait_for_burst_ack) {
                    g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: wait_for_burst_ack, priv->sequence_number: %d", priv->sequence_number);
                    break;
                }
                
                if (status < 0 ) {
                    g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FU_HPI_CFU_STATE_STATE_ERROR");
                    priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
                }
                else {
                    //if(report_id == FWUPDATE_CONTENT_REPORT_ID)
                    if(report_id == 0x25) {
                        g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: report_id: %X",report_id==DEFAULT_CONTENT_OUTPUT_USAGE);
                        switch( (unsigned char) status )
                        {
                            case FIRMWARE_UPDATE_OFFER_SKIPPED:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_SKIPPED");
                                //cfu_obj->current_offer->offer_skipped=1;
                                priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                                break;
                                
                            case FIRMWARE_UPDATE_OFFER_ACCEPTED:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_ACCEPTED");
                                if(lastpacket) {
                                    g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_ACCEPTED last_packet_sent");
                                    priv->state = FU_HPI_CFU_STATE_UPDATE_SUCCESS;
                                }
                                else
                                    priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                                break;
                                
                            case FIRMWARE_UPDATE_OFFER_REJECTED:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_REJECTED");
                                
                                //cfu_obj->current_offer->offer_rejected = 1;
                                priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                                break;
                                
                            case FIRMWARE_UPDATE_OFFER_BUSY:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_BUSY");
                                //cfu_obj->current_offer->offer_skipped = 1;
                                priv->state = FU_HPI_CFU_STATE_NOTIFY_ON_READY;
                                break;
                                
                            case FIRMWARE_UPDATE_OFFER_COMMAND_READY:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_COMMAND_READY");
                                //cfu_obj->current_offer->offer_skipped=0;
                                priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                                break;
                            
                            default:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FU_HPI_CFU_STATE_STATE_ERROR");
                                priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
                                break;
                        }
                    }
                    else if(report_id == 0x22) {
                        g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: report_id: 0x22");
                        
                        switch( (unsigned char) status ) {
                            case FIRMWARE_UPDATE_STATUS_ERROR_COMPLETE:
                            case FIRMWARE_UPDATE_STATUS_ERROR_CRC:
                            case FIRMWARE_UPDATE_STATUS_ERROR_INVALID:
                            case FIRMWARE_UPDATE_STATUS_ERROR_INVALID_ADDR:
                            case FIRMWARE_UPDATE_STATUS_ERROR_NO_OFFER:
                            case FIRMWARE_UPDATE_STATUS_ERROR_PENDING:
                            case FIRMWARE_UPDATE_STATUS_ERROR_PREPARE:
                            case FIRMWARE_UPDATE_STATUS_ERROR_SIGNATURE:
                            case FIRMWARE_UPDATE_STATUS_ERROR_VERIFY:
                            case FIRMWARE_UPDATE_STATUS_ERROR_VERSION:
                            case FIRMWARE_UPDATE_STATUS_ERROR_WRITE:
                                priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
                                //priv->offer_rejected = 1;
                                //g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: CONTENT UPDATE FAILED: Reason: '%s'", ContentResponseToString((UINT32) status));
                            break;
                            
                            case FIRMWARE_UPDATE_STATUS_SUCCESS:
                                g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: CONTENT UPDATE SUCCESS");
                                if(lastpacket)
				{
                                    priv->state = FU_HPI_CFU_STATE_UPDATE_SUCCESS;
				}
                                else
                                    priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
                                break;
                        }
                    }
                }
                break;
                
                case FU_HPI_CFU_STATE_UPDATE_SUCCESS:
                    //priv->offer_rejected = 1;  //stop sending same image next time
                    
                    //if(!all_offer_rejected(cfu_obj))
                    if(!lastpacket)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
                    else
                        priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
                    break;
                    
                case FU_HPI_CFU_STATE_UPDATE_OFFER_REJECTED:
                    //if(!all_offer_rejected(cfu_obj))
                    if(!lastpacket)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
                    else
                        priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
                    break;
                    
                case FU_HPI_CFU_STATE_OFFER_SKIP:
                    if(!lastpacket)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
                    else
                        priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
                    break;
                    
                case FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS");
                    if(!lastpacket)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
                    else
                        priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
                    break;
                    
                case FU_HPI_CFU_STATE_END_OFFER_LIST:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_END_OFFER_LIST");
                    reply = fu_hpi_cfu_send_end_offer_list(self, error);
                    priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED;
                    break;
                    
                case FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED");
                    
                    reply = fu_hpi_cfu_end_offer_list_accepted(self, error);
                    if(reply == FALSE)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
                    
                    priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN;
                    break;
                    
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN");
                    
                    reply = fu_hpi_cfu_send_start_offer_list(self, error);
                    if(reply < 0) {
                        priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
                        break;
                    }
                    priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED;
                    break;
                    
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED");
                
                    if( fu_hpi_cfu_send_offer_list_accepted(self, error) >= 0)
                        priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER;
                    else
                        priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
                    break;
                
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER");
                
                    //send the same offer again
                    reply = fu_hpi_cfu_send_offer_update_command(self, error, fw_offer);
                    if( reply < 0)
                        priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
                    else
                        priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED;
                    break;
                
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED:
                    //reply status must be SWAP_PENDING
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED");
                    reply = fu_hpi_cfu_firmware_update_offer_accepted(self, error);
                    
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED: reply: %x", reply);

                    if(reply == FIRMWARE_UPDATE_OFFER_ACCEPTED) {
                        g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED: expcetd a reject with SWAP PENDING");
                        priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST;
                        break;
                    }
                    else {
                        if(fu_hpi_cfu_firmware_update_offer_rejected(reply)) {
                            g_debug("CFU STATE: CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT: reply: %x,  OFFER_REJECTED: Reason: '%s'", reply, fu_hpi_cfu_reject_reason(reason) );
                            
                            switch(reason) {
                            
                                case FIRMWARE_OFFER_REJECT_OLD_FW:
                                    g_debug("CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT:FIRMWARE_OFFER_REJECT_OLD_FW: expcetd a reject with SWAP PENDING");
                                    break;
                            
                                case FIRMWARE_OFFER_REJECT_INV_MCU:
                                    g_debug("CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT:FIRMWARE_OFFER_REJECT_INV_MCU: expcetd a reject with SWAP PENDING");
                                    break;
                            
                                case FIRMWARE_UPDATE_OFFER_SWAP_PENDING:
                                    //device_reset = 1;
                                    g_debug("FIRMWARE_UPDATE_OFFER_SWAP_PENDING: FIRMWARE UPDATE COMPLETED.");
                                    //reset_pending = 1;
                                    break;
                            
                                case FIRMWARE_OFFER_REJECT_MISMATCH:
                                    g_debug("CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT:FIRMWARE_OFFER_REJECT_MISMATCH: expcetd a reject with SWAP PENDING");
                                    break;
                            
                                case FIRMWARE_OFFER_REJECT_BANK:
                                    g_debug("FIRMWARE_OFFER_REJECT_BANK: expcetd a reject with SWAP PENDING");
                                    break;
                            
                                case FIRMWARE_OFFER_REJECT_PLATFORM:
                                    g_debug("FIRMWARE_OFFER_REJECT_PLATFORM: expcetd a reject with SWAP PENDING");
                                    break;
                            
                                case FIRMWARE_OFFER_REJECT_MILESTONE:
                                    g_debug("FIRMWARE_OFFER_REJECT_MILESTONE: expcetd a reject with SWAP PENDING");
                                    break;
                                    
                                case FIRMWARE_OFFER_REJECT_INV_PCOL_REV:
                                    g_debug("FIRMWARE_OFFER_REJECT_INV_PCOL_REV: expcetd a reject with SWAP PENDING");
                                    priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST;
                                    break;
                                
                                case FIRMWARE_OFFER_REJECT_VARIANT:
                                    g_debug("FIRMWARE_OFFER_REJECT_VARIANT: expcetd a reject with SWAP PENDING");
                                    break;
                                
                                default:
                                    g_debug("CFU STATE: CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT: expcetd a reject with SWAP PENDING\n");
                                    break;
                            }//swicth
                        } //rejected
                        priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST;
                    }
                    break;
                    
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST:
                    g_debug("CFU STATE: CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST");

                    reply = fu_hpi_cfu_send_end_offer_list(self, error);
                    priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED;
                    break;
                    
                case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED");
                    reply = fu_hpi_cfu_end_offer_list_accepted(self, error);
                    priv->state = FU_HPI_CFU_STATE_WAIT_DEVICE_REBOOT;
                    break;
                    
                case FU_HPI_CFU_STATE_WAIT_DEVICE_REBOOT:
                    g_debug("WAITING FOR DEVICE TO RESET..! PLEASE WAIT...!");
                    priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
                    device_reset = 1;
                    break;
                    
                case FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR:
                    g_debug("CFU STATE: CFU_FIRMWARE_UPDATE_VERIFY_ERROR.  Error happened during after update verfication'.");
                    priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
                    break;
                    
                case FU_HPI_CFU_STATE_UPDATE_ALL_OFFER_REJECTED:
                    //if(all_offer_rejected(cfu_obj))
                    if(lastpacket)
                        priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
                    else
                        priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
                    break;
                    
                case FU_HPI_CFU_STATE_NOTIFY_ON_READY:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_NOTIFY_ON_READY");
                    priv->state = FU_HPI_CFU_STATE_WAIT_FOR_READY_NOTIFICATION;
                    break;
                    
                case FU_HPI_CFU_STATE_WAIT_FOR_READY_NOTIFICATION:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_WAIT_FOR_READY_NOTIFICATION");
                    priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
                    break;
                    
                case FU_HPI_CFU_STATE_UPDATE_STOP:
                    g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_STOP");
                    break_loop = 1;
                    break;

                case FU_HPI_CFU_STATE_STATE_ERROR:
                    g_debug("CFU STATE: CFU_STATE_ERROR. ERROR IN COMMUNICATION.");
                    break_loop = 1;
                    break;
        }

        if(break_loop)
            break;

    } //while
    

    /* success */
    return TRUE;
}

static gboolean
fu_hpi_cfu_device_ensure_interface(FuHpiCfuDevice *self, GError **error)
{
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    g_autoptr(GError) error_local = NULL;
    
    priv->iface_number = 0;
    
    /* release interface */
    if (priv->claimed_interface) {
        g_autoptr(GError) error_local = NULL;
        
        if (!g_usb_device_release_interface(usb_device,
                                                (gint)priv->iface_number,
                                                0,
                                                &error_local)) {
                if (!g_error_matches(error_local,
                                        G_USB_DEVICE_ERROR,
                                        G_USB_DEVICE_ERROR_NO_DEVICE)) {
                    g_warning("failed to release interface with error: %s", error_local->message);
                }
        }
        priv->claimed_interface = FALSE;
        return FALSE;
    }
    
    /* claim, without detaching kernel driver */
    if (!g_usb_device_claim_interface(usb_device,
                                        (gint)priv->iface_number,
                                        G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
                                        &error_local)) {
	    g_warning("cannot claim interface %d: %s", priv->iface_number, error_local->message);
            return FALSE;
    }
    
    /* success */
    priv->claimed_interface = TRUE;
    
    return TRUE;
}

static gboolean
fu_hpi_cfu_device_open(FuDevice *device, GError **error)
{
    FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    
    g_return_val_if_fail(FU_HPI_CFU_DEVICE(device), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    
    /* FuUsbDevice->open */
    if (!FU_DEVICE_CLASS(fu_hpi_cfu_device_parent_class)->open(device, error))
        return FALSE;
        
    /* ensure interface is claimed */
    if (!fu_hpi_cfu_device_ensure_interface(self, error))
        return FALSE;
        
    return TRUE;
}


static gboolean
fu_hpi_cfu_device_close(FuDevice *device, GError **error)
{
    FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(device));
    g_autoptr(GError) error_local = NULL;
    
    /* release interface */
    if (priv->claimed_interface) {
        
        g_autoptr(GError) error_local = NULL;
        
        if (!g_usb_device_release_interface(usb_device,
                                                (gint)priv->iface_number,
                                                0,
                                                &error_local)) {
            if (!g_error_matches(error_local,
                        G_USB_DEVICE_ERROR,
                        G_USB_DEVICE_ERROR_NO_DEVICE)) {
                g_warning("failed to release interface with: %s", error_local->message);
            }
        }
        priv->claimed_interface = FALSE;
    }
    
    /* FuUsbDevice->close */
    if(!FU_DEVICE_CLASS(fu_hpi_cfu_device_parent_class)->close(device, error))
        return FALSE;
        
    /* success */
    return TRUE;
}

static gboolean
fu_hpi_cfu_device_setup(FuDevice *device, GError **error)
{
    FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    
    g_return_val_if_fail(FU_HPI_CFU_DEVICE(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    
    GUsbDevice *usb_device = fu_usb_device_get_dev(FU_USB_DEVICE(self));
    gsize actual_length = 0;
    guint8 buf[60];
    g_autoptr(GError) error_local = NULL;
    g_autoptr(GBytes) device_version_response = NULL;
    
    /* FuHidDevice->setup */
    if (!FU_DEVICE_CLASS(fu_hpi_cfu_device_parent_class)->setup(device, error))
        return FALSE;
        
    if (!g_usb_device_control_transfer(usb_device,
                                            G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
                                            G_USB_DEVICE_REQUEST_TYPE_VENDOR,
                                            G_USB_DEVICE_RECIPIENT_DEVICE,
                                            GET_REPORT,
                                            FEATURE_REPORT_TYPE|VERSION_REPORT_ID,
                                            priv->iface_number,
                                            buf,
                                            sizeof(buf),
                                            &actual_length,
                                            FU_HPI_CFU_DEVICE_TIMEOUT,
                                            NULL,
                                            error)) {
	    g_warning("failed fu_hpi_cfu_device_setup with error: %s", error_local->message);
        return FALSE;
    }

    device_version_response = g_bytes_new(buf, actual_length);
    fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_device_setup: bytes received", device_version_response);

    g_debug("Device Version: %02X.%02X.%02X.%02X", buf[8], buf[7], buf[6], buf[5]);
    
    fu_device_set_version(device, g_strdup_printf("%02X.%02X.%02X.%02X", buf[8], buf[7], buf[6], buf[5]));
     
    // Header is 4 bytes.
    const int versionTableOffset = 4;
    
    //Component ID is 6th byte.
    const int componentIDOffset = 5;
    
    // Each component takes up 8 bytes.
    const int componentDataSize = 8;
    
    // componentIndex - refers, if having multiple offers.
    // If offers count is 3, componentIndex starts from 0.
    int componentIndex = 0; //Hardcoded to zero, since multiple offers logic is in progress.
    
    priv->burst_acksize = (int) buf[versionTableOffset + componentIndex * componentDataSize + componentIDOffset];
    g_debug("fu_hpi_cfu_device_setup: burst_acksize=%d",priv->burst_acksize);
    
    /* success */
    return TRUE;
}


static void
fu_hpi_cfu_device_version_notify_cb(FuDevice *device, GParamSpec *pspec, gpointer user_data)
{
    fw_version_device = fu_device_get_version(device);
    g_autoptr(GString) str = g_string_new(NULL);

    if(device_reset) {
        g_autoptr(GError) error_local = NULL;
        
        if (g_strcmp0(fw_version_device, fw_version_payload) != 0) {
                g_warning(error_local,
                            FWUPD_ERROR,
                            FWUPD_ERROR_WRITE,
                            "firmware version failed %s != %s",
                            fw_version_payload,
                            fw_version_device);
                return FALSE;
        }
        g_debug("firmware update is success with version %s", fw_version_device);
    }
}

static gboolean
fu_hpi_cfu_device_write_firmware(FuDevice *device,
                    FuFirmware *firmware,
                    FuProgress *progress,
                    FwupdInstallFlags flags,
                    GError **error)
{
    fu_progress_set_id(progress, G_STRLOC);
    fu_progress_set_percentage(progress, 0);
    fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 95, "write");
    fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 5, "reset");


    FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
    g_autoptr(FuFirmware) firmware_archive = fu_archive_firmware_new();
    g_autoptr(FuFirmware) fw_offer = NULL;
    g_autoptr(FuFirmware) fw_payload = NULL;
    g_autoptr(GBytes) blob_offer = NULL;
    g_autoptr(GBytes) blob_payload = NULL;
   

    /* get both images */
    fw_offer = fu_archive_firmware_get_image_fnmatch(FU_ARCHIVE_FIRMWARE(firmware), "*.offer.bin", error);
    if (fw_offer == NULL)
        return FALSE;
        
    fw_payload = fu_archive_firmware_get_image_fnmatch(FU_ARCHIVE_FIRMWARE(firmware), "*.payload.bin", error);
    if (fw_payload == NULL)
        return FALSE;

    /* cfu state machine implementation */
    fu_progress_set_status(progress, FWUPD_STATUS_DEVICE_WRITE);
    if( !fu_hpi_cfu_update_firmware_state_machine(self, error, progress, fw_offer, fw_payload))
        return FALSE;
    fu_progress_step_done(progress);
    
    fu_progress_set_status(progress, FWUPD_STATUS_DEVICE_RESTART);
    /* the device automatically reboots */
    fu_device_add_flag(device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
    fu_progress_step_done(progress);

    
    /* done */ 
    fu_progress_set_id(progress, G_STRLOC);
    fu_progress_set_percentage(progress, 100);

    return TRUE;
}

static void
fu_hpi_cfu_device_init(FuHpiCfuDevice *self)
{
    FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
    
    
    priv->iface_number = 0x00;
    priv->state = FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION;
    
    fu_device_add_protocol(FU_DEVICE(self), "com.microsoft.cfu");
    fu_device_set_version_format(FU_DEVICE(self), FWUPD_VERSION_FORMAT_QUAD);
    fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UPDATABLE);
    fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_REQUIRE_AC);
    fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UNSIGNED_PAYLOAD);
    fu_device_set_firmware_gtype(FU_DEVICE(self), FU_TYPE_ARCHIVE_FIRMWARE);

    /* the HP Fleetwwod device reboot takes down the entire hub for ~240 seconds */
    fu_device_set_remove_delay(FU_DEVICE(self), 240 * 1000);
    
    if(device_reset) {
        g_signal_connect(FWUPD_DEVICE(self),
                            "notify::version",
                            G_CALLBACK(fu_hpi_cfu_device_version_notify_cb),
                            NULL);
    }
}

static void
fu_hpi_cfu_device_class_init(FuHpiCfuDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    FuDeviceClass *device_class = FU_DEVICE_CLASS(klass);
    
    device_class->open = fu_hpi_cfu_device_open;
    device_class->setup = fu_hpi_cfu_device_setup;
    device_class->write_firmware = fu_hpi_cfu_device_write_firmware;
    device_class->close = fu_hpi_cfu_device_close;
}
