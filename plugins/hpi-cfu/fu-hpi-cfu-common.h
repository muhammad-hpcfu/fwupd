/*
 * Copyright 2024 Owner Name <owner.name@hp.com>
 *
 */


#ifndef _fu_hpi_common_h_
#define _fu_hpi_common_h_


#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "fu-hpi-cfu-device.h"


/* This structure is used to read the data packet header inside the payload file */
typedef struct {
	gint32 offset;
	gchar length;
} __attribute__((packed))  FuHpiCfuPayloadHeader;


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
	guint8 buf[256];
	gint8 count;
	gint8 from_index_to_read;
} FuHpiCfuPayloadRead;

typedef enum {
	FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION = 0x01,
	FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION_ACCEPTED = 0x02,
	FU_HPI_CFU_STATE_START_OFFER_LIST = 0x03,
	FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED = 0x04,
	FU_HPI_CFU_STATE_UPDATE_OFFER = 0x05,
	FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED = 0x06,
	FU_HPI_CFU_STATE_UPDATE_CONTENT = 0x07,
	FU_HPI_CFU_STATE_UPDATE_SUCCESS = 0x08,
	FU_HPI_CFU_STATE_UPDATE_OFFER_REJECTED = 0x09,
	FU_HPI_CFU_STATE_OFFER_SKIP = 0x0A,
	FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS = 0x0B,
	FU_HPI_CFU_STATE_END_OFFER_LIST = 0x0C,
	FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED = 0x0D,
	FU_HPI_CFU_STATE_UPDATE_ALL_OFFER_REJECTED = 0x0E,
	FU_HPI_CFU_STATE_UPDATE_STOP = 0x0F,
	FU_HPI_CFU_STATE_STATE_ERROR = 0x10,
	FU_HPI_CFU_STATE_CHECK_UPDATE_CONTENT = 0x11,
	FU_HPI_CFU_STATE_NOTIFY_ON_READY = 0x12,
	FU_HPI_CFU_STATE_WAIT_FOR_READY_NOTIFICATION = 0x13,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN = 0x14,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED = 0x15,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER = 0x16,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED = 0x17,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST = 0x18,
	FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED = 0x19,
	FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR = 0x1A,
	FU_HPI_CFU_STATE_WAIT_DEVICE_REBOOT = 0x1B,
} FuHpiCfuState;



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
	gboolean device_reset;
	gint32 curfilepos;
} FuHpiCfuDevicePrivate;


typedef union _FuHpiCfuComponentVersion {
	struct {
		gint8 variant;
		gint16 minor_version;
		gint8 major_version;
		} __attribute__((packed)) header;
}__attribute__((packed)) FuHpiCfuComponentVersion;

typedef struct {
	gchar report_id;
	struct {
		gint8 segment_number;
		gint8 reserved0 : 6;
		gint8 force_immediate_reset : 1;
		gint8 force_ignore_version : 1;
		gint8 component_id;
		gint8 token;
		}__attribute__((packed)) FuHpiCfuComponentInfo;

	FuHpiCfuComponentVersion version;
	gint32 vendor_specific;
	struct {
		gint8 protocol_version : 4;
		gint8 reserved0 : 4;
		gint8 reserved1;
		gint16 vendor_specific;
	} __attribute__((packed)) FuHpiCfuMiscProtocolVersion;
} __attribute__((packed)) FuHpiCfuFwUpdateOfferCmdPkt;

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



//SOME DEFAULTS
#define DEFAULT_VERSION_FEATURE_USAGE VERSION_REPORT_ID
#define DEFAULT_CONTENT_OUTPUT_USAGE FWUPDATE_CONTENT_REPORT_ID
#define DEFAULT_CONTENT_RESPONSE_INPUT_USAGE FWUPDATE_CONTENT_REPORT_ID
#define DEFAULT_OFFER_OUTPUT_USAGE FIRMWARE_UPDATE_OFFER_ID
#define DEFAULT_OFFER_RESPONSE_INPUT_USAGE FIRMWARE_UPDATE_OFFER_ID



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


#endif
