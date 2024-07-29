/*
 * Copyright 2024 Owner Name <owner.name@hp.com>
 *
 */
 
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "fu-cfu-struct.h"
#include "fu-hpi-cfu-common.h"
#include "fu-hpi-cfu-device.h"

G_DEFINE_TYPE_WITH_PRIVATE(FuHpiCfuDevice, fu_hpi_cfu_device, FU_TYPE_HID_DEVICE)
#define GET_PRIVATE(o) (fu_hpi_cfu_device_get_instance_private(o))

#define FU_HPI_CFU_DEVICE_TIMEOUT 0 /* ms */

static gboolean
fu_hpi_cfu_start_entire_transaction(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) start_entire_trans_request = NULL;
	guint8 start_entire_transaction_buf[] = {0x25,0x00,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	start_entire_trans_request = g_bytes_new(start_entire_transaction_buf, sizeof(start_entire_transaction_buf));
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_start_entire_transaction sending:", start_entire_trans_request);

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_HOST_TO_DEVICE,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    SET_REPORT,
					    OUT_REPORT_TYPE | START_ENTIRE_TRANSACTION_REPORT_ID,
					    0,
					    start_entire_transaction_buf,
					    sizeof(start_entire_transaction_buf),
					    NULL,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_warning("failed send_start_entire_transaction: %s", error_local->message);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_hpi_cfu_start_entire_transaction_accepted(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	gsize actual_length = 0;
	guint8 buf[128] = {0};
	g_autoptr(GBytes) start_entire_trans_response = NULL;

	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      0x81,
					      buf,
					      sizeof(buf),
					      &actual_length,
					      FU_HPI_CFU_DEVICE_TIMEOUT,
					      NULL,
					      &error_local)) {
		g_warning("failed start_entire_transaction_accepted: %s", error_local->message);
		return FALSE;
	}

	g_debug("fu_hpi_cfu_start_entire_transaction_accepted: total bytes received: %x", (guint)actual_length);

	start_entire_trans_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN, "bytes received", start_entire_trans_response);

	/* success */
	if (buf[13] == 0x01) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
fu_hpi_cfu_send_start_offer_list(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) start_offer_list_request = NULL;

	guint8 start_offer_list_buf[] = {0x25,0x01,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	start_offer_list_request = g_bytes_new(start_offer_list_buf, sizeof(start_offer_list_buf));
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_start_offer_list: sending", start_offer_list_request);

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_HOST_TO_DEVICE,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    SET_REPORT,
					    OUT_REPORT_TYPE | OFFER_INFO_START_OFFER_LIST_ID,
					    0,
					    start_offer_list_buf,
					    sizeof(start_offer_list_buf),
					    NULL,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_warning("failed to do send_start_offer_list: %s", error_local->message);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gint8
fu_hpi_cfu_send_offer_list_accepted(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	gsize actual_length = 0;
	guint8 buf[128] = {0};
	g_autoptr(GBytes) send_offer_list_response = NULL;

	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      0x81,
					      buf,
					      sizeof(buf),
					      &actual_length,
					      FU_HPI_CFU_DEVICE_TIMEOUT,
					      NULL,
					      &error_local)) {
		g_warning("failed to do send_offer_list_accepted: %s", error_local->message);
		return FALSE;
	}

	g_debug("fu_hpi_cfu_send_offer_list_accepted: total bytes received: %x", (guint)actual_length);
	send_offer_list_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_offer_list_accepted: bytes received", send_offer_list_response);

	/* success */
	if (buf[13] == 0x01) {
		g_debug("fu_hpi_cfu_device_send_offer_list_accepted success.");
	} else {
		if (buf[13] == 0x02) {
			g_warning("failed fu_hpi_cfu_device_send_offer_list_accepted with reason: %s", fu_cfu_rr_code_to_string(buf[9]));

		} else {
			g_warning("failed fu_hpi_cfu_device_send_offer_list_accepted with reason: %s", fu_cfu_rr_code_to_string(buf[9]));
		}
	}
	return buf[13];
}

static gint8
fu_hpi_cfu_send_offer_update_command(FuHpiCfuDevice *self, FuFirmware *fw_offer)
{
	g_autoptr(GBytes) blob_offer = NULL;
	gsize size = 0;
	const guint8 *data = 0;
	FuHpiCfuFwUpdateOfferCmdPkt offer_command_packet;

	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) offer_command_request = NULL;
	gchar *fw_offer_cmd = NULL;

	blob_offer = fu_firmware_get_bytes(fw_offer, NULL);
	data = g_bytes_get_data(blob_offer, &size);
	fw_offer_cmd = (gchar *)&offer_command_packet;
	memcpy(fw_offer_cmd + 1, data, size);

	offer_command_packet.FuHpiCfuComponentInfo.force_immediate_reset = 1; // hardcoded for now (Update now)
	// offer_command_packet.FuHpiCfuComponentInfo.force_immediate_reset = 0; //hardcoded for now (Update on disconnect)
	offer_command_packet.FuHpiCfuComponentInfo.force_ignore_version = 1; // hardcoded for now (Force update version)
	offer_command_packet.report_id = OFFER_INFO_START_OFFER_LIST_ID;

	offer_command_request = g_bytes_new(fw_offer_cmd, size);
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_offer_update_command sending:", offer_command_request);

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_HOST_TO_DEVICE,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    SET_REPORT,
					    OUT_REPORT_TYPE | FIRMWARE_UPDATE_OFFER_ID,
					    0,
					    (guchar*) fw_offer_cmd,
					    size + 1,
					    NULL,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_warning("failed to do send_offer_update_command: %s", error_local->message);
		return FALSE;
	}

	/* success */
	return 0;
}

static gint8
fu_hpi_cfu_firmware_update_offer_accepted(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	gsize actual_length = 0;
	guint8 buf[128] = {0};
	g_autoptr(GBytes) offer_response = NULL;

	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      0x81,
					      buf,
					      sizeof(buf),
					      &actual_length,
					      FU_HPI_CFU_DEVICE_TIMEOUT,
					      NULL,
					      &error_local)) {
		g_warning("failed to do firmware_update_offer_accepted: %s", error_local->message);
		return FALSE;
	}

	g_debug("fu_hpi_cfu_firmware_update_offer_accepted: total bytes received: %x", (guint)actual_length);
	offer_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_firmware_update_offer_accepted: bytes received", offer_response);

	/* success */
	if (buf[13] == 0x01) {
		g_debug("fu_hpi_cfu_firmware_update_offer_accepted: success.");
	} else {
		if (buf[13] == 0x02) {
			g_debug("fu_hpi_cfu_firmware_update_offer_accepted:reason: %s",
					fu_cfu_rr_code_to_string(buf[9]));
		} else {
			g_debug("fu_hpi_cfu_firmware_update_offer_accepted:reason: %s buf[13] is not a REJECT.",
					fu_cfu_rr_code_to_string(buf[9]));
		}
	}

	return buf[13];
}

static gboolean
fu_hpi_cfu_send_firmware_update_content(FuHpiCfuDevicePrivate *priv,
					FuHpiCfuDevice *self,
					FuFirmware *fw_payload)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) header_buf = NULL;
	g_autoptr(GBytes) payload_buf = NULL;
	g_autoptr(GBytes) untransmitted_buf = NULL;
	g_autoptr(GBytes) fw_content_command = NULL;

	g_autoptr(GBytes) blob_payload = NULL;
	gsize payload_file_size = 0;
	const guint8 *data = 0;

	gint8 readlen = 0;
	gint8 sendlen = 0;
	gint32 existingbytecount = 0;
	gsize curfilefilepos = 0;
	gint32 header_len = 0;
	gint32 fillsize	= 0;

	FuHpiCfuPayloadHeader ph;
	FuHpiCfuFwUpdateContentCommand fcc;

	memset(&ph, 0, sizeof(FuHpiCfuPayloadHeader));
	memset(&fcc, 0, sizeof(FuHpiCfuFwUpdateContentCommand));


	blob_payload = fu_firmware_get_bytes(fw_payload, NULL);
	data = g_bytes_get_data(blob_payload, &payload_file_size);

	data = data + priv->curfilepos;

	g_debug("curfilepos:%d\n", priv->curfilepos);

	if (priv->pr.count > 0) {
		if (priv->pr.count < 52) {
			if (curfilefilepos >= payload_file_size) {
				sendlen = priv->pr.count;
				memcpy((guint8 *)&fcc.data[0], (guint8 *)&priv->pr.buf[priv->pr.from_index_to_read], sendlen);
				priv->pr.count = 0;
				priv->pr.from_index_to_read = 0;
			} else {
				existingbytecount = priv->pr.count;
				memcpy((guint8 *)&fcc.data[0], (guint8 *)&priv->pr.buf[priv->pr.from_index_to_read], existingbytecount);

				// now read a new header
				header_len = sizeof(FuHpiCfuPayloadHeader);
				memcpy(&ph, data, sizeof(ph));

				g_debug("fu_hpi_cfu_send_firmware_update_content header size: %d", header_len);
				header_buf = g_bytes_new(&ph, header_len);
				fu_dump_bytes(G_LOG_DOMAIN, "header", header_buf);

				data = data + header_len;
				curfilefilepos = curfilefilepos + header_len;
				readlen = (gint8)ph.length;
				g_debug("fu_hpi_cfu_send_firmware_update_content: bytes payload read: %d", readlen);
				memcpy(&priv->pr.buf, data, readlen);
				data = data + readlen;
				curfilefilepos = curfilefilepos + readlen;

				priv->pr.count = readlen;
				priv->pr.from_index_to_read = 0;
				fillsize = 52 - existingbytecount;

				if (readlen <= fillsize) {
					sendlen = readlen + existingbytecount;
					memcpy((guint8 *)&fcc.data[existingbytecount], (guint8 *)&priv->pr.buf[0], readlen);
					priv->pr.from_index_to_read += readlen;
					priv->pr.count -= readlen;
				} else {
					sendlen = 52;
					memcpy((guint8 *)&fcc.data[existingbytecount], (guint8 *)&priv->pr.buf[0], fillsize);
					priv->pr.from_index_to_read += fillsize;
					priv->pr.count -= fillsize;

					g_debug("fu_hpi_cfu_send_firmware_update_content: %d bytes of untransmitted data remain in cache",
							priv->pr.count);
					// untransmitted_buf = g_bytes_new(priv->pr.buf, priv->pr.count); 
					// fu_dump_bytes(G_LOG_DOMAIN, "untransmitted bytes", untransmitted_buf);
				}
			} //<52
		} // if(pr.count<52)
		else {	// cache has more than or equal to 52 bytes
			sendlen = 52;
			memcpy((guint8 *)&fcc.data[0], (guint8 *)&priv->pr.buf[priv->pr.from_index_to_read], sendlen);
			priv->pr.from_index_to_read = priv->pr.from_index_to_read + 52;
			priv->pr.count = priv->pr.count - 52;

			if (priv->pr.count) {
				g_debug("fu_hpi_cfu_send_firmware_update_content: %d bytes of untransmitted data remain in cache", priv->pr.count);
				// untransmitted_buf = g_bytes_new(priv->pr.buf, priv->pr.count);
				// fu_dump_bytes(G_LOG_DOMAIN, "untransmitted bytes", untransmitted_buf);
			}
		}

		// now check if no data in the buffer and end of file reached
		if (priv->pr.count == 0) {
			priv->last_packet_sent = (curfilefilepos >= payload_file_size) ? 1 : 0;
			g_debug("fu_hpi_cfu_send_firmware_update_content: priv->last_packet_sent: %d", priv->last_packet_sent);
		}
	} else { // if no bytes in the pr
		existingbytecount = 0;

		// now read a new header
		header_len = sizeof(FuHpiCfuPayloadHeader);
		memcpy(&ph, data, sizeof(ph));

		g_debug("fu_hpi_cfu_send_firmware_update_content: payload header size: %d", header_len);

		header_buf = g_bytes_new(&ph, header_len);
		fu_dump_bytes(G_LOG_DOMAIN, "header", header_buf);

		g_debug("fu_hpi_cfu_send_firmware_update_content: offset: %x, length: 0x%x", (guint)ph.offset, (guchar)ph.length);

		data = data + header_len;
		priv->curfilepos = priv->curfilepos + header_len;

		readlen = (gint8)ph.length;
		g_debug("fu_hpi_cfu_send_firmware_update_content: bytes payload read: %d", readlen);

		memcpy(&priv->pr.buf, data, readlen);

		payload_buf = g_bytes_new(priv->pr.buf, readlen);
		fu_dump_bytes(G_LOG_DOMAIN, "payload data", payload_buf);

		data = data + readlen;
		priv->curfilepos = priv->curfilepos + readlen;

		priv->pr.count = readlen;
		priv->pr.from_index_to_read = 0;
		fillsize = 52 - existingbytecount;

		if (readlen <= fillsize) {
			sendlen = readlen + existingbytecount;

			for (gint32 i = 0; i < readlen; i++)
				fcc.data[i] = priv->pr.buf[i];

			priv->pr.from_index_to_read += readlen;
			priv->pr.count -= readlen;
		} else {
			sendlen = 52;

			for (gint32 i = 0; i < readlen; i++)
				fcc.data[i] = priv->pr.buf[i];

			priv->pr.from_index_to_read += fillsize;
			priv->pr.count -= fillsize;
		}

		if (priv->pr.count) {
			g_debug("fu_hpi_cfu_send_firmware_update_content:%d bytes of untransmitted data remain in the cache",priv->pr.count);
		}

		// now check if no data in the buffer and end of file reached
		if (priv->pr.count == 0) {
			priv->last_packet_sent = (priv->curfilepos >= (gint32)payload_file_size) ? 1 : 0;
			g_debug("fu_hpi_cfu_send_firmware_update_content: priv->last_packet_sent:%d", priv->last_packet_sent);
		}
	}

	fcc.report_id = FWUPDATE_CONTENT_REPORT_ID;

	if (priv->sequence_number == 0)
		fcc.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
	else
		fcc.flags = (priv->last_packet_sent) ? FIRMWARE_UPDATE_FLAG_LAST_BLOCK : 0;

	fcc.length = sendlen;
	fcc.sequence_number = ++priv->sequence_number;
	fcc.address = priv->currentaddress;

	priv->currentaddress += sendlen;
	priv->bytes_sent += sendlen;
	priv->bytes_remaining = priv->payload_file_size - (priv->bytes_sent + header_len);

	g_debug("fu_hpi_cfu_send_firmware_update_content: priv->sequence_number:%d", priv->sequence_number);
	g_debug("fu_hpi_cfu_send_firmware_update_content: priv->bytes_sent:%d", priv->bytes_sent);
	g_debug("fu_hpi_cfu_send_firmware_update_content: priv->bytes_remaining:%d", priv->bytes_remaining);

	if (priv->last_packet_sent) {
		fcc.flags = FIRMWARE_UPDATE_FLAG_LAST_BLOCK;
	}

	g_debug("================== content data ==================");
	g_debug("fcc.report_id:%x", (guchar)fcc.report_id);
	g_debug("fcc.flags:%x", (guchar)fcc.flags);
	g_debug("fcc.length:%d", fcc.length);
	g_debug("fcc.sequence_number:%02x", (gushort)fcc.sequence_number);
	g_debug("fcc.address:%08x", (guchar)fcc.address);

	g_debug("content pkt size=%x", (guint)sizeof(FuHpiCfuFwUpdateContentCommand));

	fw_content_command = g_bytes_new(&fcc, sizeof(FuHpiCfuFwUpdateContentCommand));
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_send_firmware_update_content: bytes sent", fw_content_command);

	g_debug("================== content data end==================");

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_HOST_TO_DEVICE,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    SET_REPORT,
					    OUT_REPORT_TYPE | FWUPDATE_CONTENT_REPORT_ID,
					    0,
					    (guint8 *)&fcc,
					    sizeof(FuHpiCfuFwUpdateContentCommand),
					    NULL,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_warning("failed to send update_content command:%s", error_local->message);
		return FALSE;
	}
	return TRUE;
}

static gint8
fu_hpi_cfu_read_content_ack(FuHpiCfuDevicePrivate *priv,
				FuHpiCfuDevice *self,
				gint *lastpacket,
				gint *report_id,
				gint *reason)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) read_ack_response = NULL;
	gsize actual_length = 0;
	guint8 buf[128];
	*report_id = 0;

	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      0x81,
					      buf,
					      sizeof(buf),
					      &actual_length,
					      FU_HPI_CFU_DEVICE_TIMEOUT,
					      NULL,
					      &error_local)) {
		g_warning("failed to send update_content command:%s", error_local->message);
		return FALSE;
	}
	g_debug("fu_hpi_cfu_read_content_ack: bytes_received:%x", (guint)actual_length);
	read_ack_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN,"fu_hpi_cfu_read_content_ack: bytes received",read_ack_response);

	*report_id = buf[0];
	/* success */
	if (buf[0] == FWUPDATE_CONTENT_REPORT_ID) {
		g_debug("status:%s response:%s", fu_cfu_offer_status_to_string(buf[13]),
				fu_cfu_rr_code_to_string(buf[9]));


		if (buf[13] == 0x01) {
			if (priv->last_packet_sent == 1)
				*lastpacket = 1;

			g_debug("fu_hpi_cfu_read_content_ack: last_packet_sent:1");

			return (gint8)buf[13];
		}
		return (gint8)buf[13];
	} else {
		g_debug("read_content_ack: buffer[5]: %02x, response:%s",
				(guchar)buf[5], fu_cfu_content_status_to_string(buf[5]));

		if (buf[5] == 0x00) {
			g_debug("read_content_ack:1");

			if (priv->last_packet_sent == 1)
				*lastpacket = 1;

			g_debug("read_content_ack:last_packet_sent:%d", *lastpacket);
			return (gint8)buf[5];
		}
		else {

			return (gint8)buf[5];
		}
	}
	return -1;
}

static gboolean
fu_hpi_cfu_send_end_offer_list(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GBytes) end_offer_request = NULL;

	guint8 end_offer_list_buf[] = {0x25,0x02,0x00,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	g_debug("fu_hpi_cfu_send_end_offer_list sending:");
	end_offer_request = g_bytes_new(end_offer_list_buf, sizeof(end_offer_list_buf));
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_start_entire_transaction sending:", end_offer_request);

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_HOST_TO_DEVICE,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    SET_REPORT,
					    OUT_REPORT_TYPE | OFFER_INFO_END_OFFER_LIST_ID,
					    0,
					    end_offer_list_buf,
					    sizeof(end_offer_list_buf),
					    NULL,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_warning("failed to send_end_offer_list:%s", error_local->message);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gint8
fu_hpi_cfu_end_offer_list_accepted(FuHpiCfuDevice *self)
{
	g_autoptr(GError) error_local = NULL;
	gsize actual_length = 0;
	guint8 buf[128] = {0};
	g_autoptr(GBytes) end_offer_response = NULL;

	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      0x81,
					      buf,
					      sizeof(buf),
					      &actual_length,
					      FU_HPI_CFU_DEVICE_TIMEOUT,
					      NULL,
					      &error_local)) {
		g_warning("failed to do end_offer_list_accepted:%s", error_local->message);
		return FALSE;
	}

	g_debug("fu_hpi_cfu_end_offer_list_accepted: bytes_received:%x", (guint)actual_length);
	end_offer_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_end_offer_list_accepted: bytes received", end_offer_response);

	g_debug("fu_hpi_cfu_end_offer_list_accepted: Identify Type buffer[4]:%02x", (guchar)buf[4]);
	g_debug("fu_hpi_cfu_end_offer_list_accepted: reject reason:buffer[9]:%02x, this value is meaningful when buffer[13]=2", (guchar)buf[9]);
	g_debug("fu_hpi_cfu_end_offer_list_accepted: reply status:buffer[13]:%02x, response:%s", (guchar)buf[13], fu_cfu_rr_code_to_string(buf[13]));

	/* success */
	if (buf[13] == 0x01) {
		g_debug("fu_hpi_cfu_end_offer_list_accepted: buffer[13]:1");
	} else {
		if (buf[13] == 0x02) {
			g_warning("fu_hpi_cfu_firmware_update_offer_accepted: not acceptance with reason:%s",
					fu_cfu_rr_code_to_string(buf[9]));
		} else {
			g_warning("fu_hpi_cfu_firmware_update_offer_accepted: not acceptance with reason:%s but buf[13] is not REJECT",
					fu_cfu_rr_code_to_string(buf[9]));
		}
	}

	return buf[13];
}

static gint8
fu_hpi_cfu_firmware_update_offer_rejected(gint8 reply)
{
	gint8 ret = 0;

	if (reply == FU_HPI_CFU_STATE_UPDATE_OFFER_REJECTED)
		ret = 1;

	return ret;
}


static gboolean
fu_hpi_cfu_update_firmware_state_machine(FuHpiCfuDevice *self,
					 FuProgress *progress,
					 FuFirmware *fw_offer,
					 FuFirmware *fw_payload)
{
	FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);
	

	gsize payload_file_size = 0;
	gint8 reply = 0;
	gint32 wait_for_burst_ack = 0;
	gint32 status = 0;
	gint32 report_id = 0;
	gint32 reason = 0;
	gint32 lastpacket = 0;
	gint32 break_loop = 0;

	g_autoptr(GBytes) blob_payload = NULL;
	blob_payload = fu_firmware_get_bytes(fw_payload, NULL);
	g_bytes_get_data(blob_payload, &payload_file_size);

	priv->curfilepos = 0;
	priv->payload_file_size = payload_file_size;

	fu_progress_set_percentage(progress, 0);
	g_debug("payload_file_size: %lu\n", payload_file_size);

	while (1) {
		switch (priv->state) {
		case FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION");

			if (!fu_hpi_cfu_start_entire_transaction(self))
				priv->state = FU_HPI_CFU_STATE_STATE_ERROR;

			priv->state = FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_START_ENTIRE_TRANSACTION_ACCEPTED:
			g_debug("priv->burst_acksize:%d", priv->burst_acksize);

			if (!fu_hpi_cfu_start_entire_transaction_accepted(self))
				priv->state = FU_HPI_CFU_STATE_STATE_ERROR;

			priv->state = FU_HPI_CFU_STATE_START_OFFER_LIST;
			fu_progress_step_done(progress); /* start-entire  */
			break;

		case FU_HPI_CFU_STATE_START_OFFER_LIST:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_START_OFFER_LIST");

			if (!fu_hpi_cfu_send_start_offer_list(self))
				priv->state = FU_HPI_CFU_STATE_STATE_ERROR;

			priv->state = FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_START_OFFER_LIST_ACCEPTED");

			if (fu_hpi_cfu_send_offer_list_accepted(self) >= 0)
				priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
			else
				priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
			fu_progress_step_done(progress); /* start-offer */
			break;

		case FU_HPI_CFU_STATE_UPDATE_OFFER:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_OFFER");

			if (fu_hpi_cfu_send_offer_update_command(self, fw_offer) >= 0)
				priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED;
			else
				priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
			break;

		case FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED");

			reply = fu_hpi_cfu_firmware_update_offer_accepted(self);

			if (reply == FIRMWARE_UPDATE_OFFER_ACCEPT) {
				g_debug("CFU STATE: CFU_FIRMWARE_UPDATE_OFFER_ACCEPT_3: reply:%d,OFFER_ACCEPTED", reply);
				// cfu_obj->current_offer->offer_in_progress = 1;
				// cfu_obj->current_offer->offer_rejected = 0;
				// cfu_obj->current_offer->offer_skipped = 0;
				priv->sequence_number = 0;
				priv->currentaddress = 0;
				priv->last_packet_sent = 0;
				priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
			} else {
				// cfu_obj->current_offer->offer_in_progress = 0;
				// cfu_obj->current_offer->offer_skipped = 0;
				// cfu_obj->current_offer->offer_skipped = 0;

				if (reply == FIRMWARE_UPDATE_OFFER_SKIP) {
					// cfu_obj->current_offer->offer_skipped = 1;
					g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply:%d, OFFER_SKIPPED", reply);
					priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
				} else if (reply == FIRMWARE_UPDATE_OFFER_REJECT) {
					// cfu_obj->current_offer->offer_rejected=1;
					g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply:%d, OFFER_REJECTED", reply);
					priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
				} else if (reply == FIRMWARE_UPDATE_OFFER_BUSY) {
					// cfu_obj->current_offer->offer_skipped = 1;
					g_debug("FU_HPI_CFU_STATE_UPDATE_OFFER_ACCEPTED: reply:%d, OFFER_BUSY", reply);
					g_print("Offer busy. Please turn off and turn on the device.\n");
					priv->state = FU_HPI_CFU_STATE_NOTIFY_ON_READY;
				} else {
					// send a new offer from the list
					priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
				}
			}
			fu_progress_step_done(progress); /* send-offer */
			break;

		case FU_HPI_CFU_STATE_UPDATE_CONTENT:

			reply = fu_hpi_cfu_send_firmware_update_content(priv, self, fw_payload);
			priv->state = FU_HPI_CFU_STATE_CHECK_UPDATE_CONTENT;
			fu_progress_set_percentage_full(progress, priv->bytes_sent, priv->payload_file_size);

			if (reply == FALSE) {
				g_debug("Error sending payload command for sequence number:%d", priv->sequence_number);
				priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
				break;
			}
			break;

		case FU_HPI_CFU_STATE_CHECK_UPDATE_CONTENT:

			lastpacket = 0;
			wait_for_burst_ack = 0;

			if (priv->last_packet_sent) {
				g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT:last_packet_sent");
				status = fu_hpi_cfu_read_content_ack(priv,
						self,
						&lastpacket,
						&report_id,
						&reason);
			} else {
				switch (priv->burst_acksize) {
				case 1:
					if ((priv->sequence_number) % 16 == 0) {
						status = fu_hpi_cfu_read_content_ack(priv,
								self,
								&lastpacket,
								&report_id,
								&reason);
					} else {
						priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
						wait_for_burst_ack = 1;
					}
					break;

				case 2:
					if ((priv->sequence_number) % 32 == 0) {
						status = fu_hpi_cfu_read_content_ack(priv,
								self,
								&lastpacket,
								&report_id,
								&reason);
					} else {
						priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
						wait_for_burst_ack = 1;
					}
					break;

				case 3:
					if ((priv->sequence_number) % 64 == 0) {
						status = fu_hpi_cfu_read_content_ack(priv,
								self,
								&lastpacket,
								&report_id,
								&reason);
					} else {
						priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
						wait_for_burst_ack = 1;
					}
					break;

				default:
					status = fu_hpi_cfu_read_content_ack(priv,
							self,
							&lastpacket,
							&report_id,
							&reason);
				}
			}

			if (wait_for_burst_ack) {
				g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT:wait_for_burst_ack,priv->sequence_number:%d", priv->sequence_number);
				break;
			}

			if (status < 0 ) {
				g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FU_HPI_CFU_STATE_STATE_ERROR");
				priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
			} else {
				// if(report_id == FWUPDATE_CONTENT_REPORT_ID)
				if (report_id == 0x25) {
					g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT:report_id:%d", report_id == DEFAULT_CONTENT_OUTPUT_USAGE);
					switch (status) {
					case FIRMWARE_UPDATE_OFFER_SKIP:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_SKIPPED");
						// cfu_obj->current_offer->offer_skipped=1;
						priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
						break;

					case FIRMWARE_UPDATE_OFFER_ACCEPT:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_ACCEPTED");
						if (lastpacket) {
							g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: OFFER_ACCEPTED last_packet_sent");
							priv->state = FU_HPI_CFU_STATE_UPDATE_SUCCESS;
						} else
							priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
						break;

					case FIRMWARE_UPDATE_OFFER_REJECT:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_REJECTED");

						// cfu_obj->current_offer->offer_rejected = 1;
						priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
						break;

					case FIRMWARE_UPDATE_OFFER_BUSY:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_BUSY");
						// cfu_obj->current_offer->offer_skipped = 1;
						priv->state = FU_HPI_CFU_STATE_NOTIFY_ON_READY;
						break;

					case FIRMWARE_UPDATE_OFFER_COMMAND_READY:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FIRMWARE_UPDATE_OFFER_COMMAND_READY");
						// cfu_obj->current_offer->offer_skipped=0;
						priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
						break;

					default:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FU_HPI_CFU_STATE_STATE_ERROR");
						priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
						break;
					}
				} else if (report_id == 0x22) {
					g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT:report_id:0x22");

					switch (status) {
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
						priv->state = FU_HPI_CFU_STATE_STATE_ERROR;
						// priv->offer_rejected = 1;
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: FAILED, reason:%s",
								fu_cfu_content_status_to_string(status));
						break;

					case FIRMWARE_UPDATE_STATUS_SUCCESS:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: CONTENT UPDATE SUCCESS");
						if (lastpacket) {
							priv->state = FU_HPI_CFU_STATE_UPDATE_SUCCESS;
						} else
							priv->state = FU_HPI_CFU_STATE_UPDATE_CONTENT;
						break;

					default:
						g_debug("CFU STATE: CFU_STATE_CHECK_UPDATE_CONTENT: status none.\n");
						break;
					}
				}
			}
			break;

		case FU_HPI_CFU_STATE_UPDATE_SUCCESS:
			// priv->offer_rejected = 1;  //stop sending same image next time

			// if(!all_offer_rejected(cfu_obj))
			if (!lastpacket)
				priv->state = FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS;
			else
				priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
			break;

		case FU_HPI_CFU_STATE_UPDATE_OFFER_REJECTED:
			// if(!all_offer_rejected(cfu_obj))
			if (!lastpacket)
				priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
			else
				priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
			break;

		case FU_HPI_CFU_STATE_OFFER_SKIP:
			if (!lastpacket)
				priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
			else
				priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
			break;

		case FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_UPDATE_MORE_OFFERS");
			if (!lastpacket)
				priv->state = FU_HPI_CFU_STATE_UPDATE_OFFER;
			else
				priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST;
			break;

		case FU_HPI_CFU_STATE_END_OFFER_LIST:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_END_OFFER_LIST");
			reply = fu_hpi_cfu_send_end_offer_list(self);
			priv->state = FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_END_OFFER_LIST_ACCEPTED");

			reply = fu_hpi_cfu_end_offer_list_accepted(self);
			if (reply == FALSE)
				priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;

			priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN;
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_BY_SENDING_OFFER_LIST_AGAIN");

			reply = fu_hpi_cfu_send_start_offer_list(self);
			if (reply < 0) {
				priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
				break;
			}
			priv->state =
				FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_LIST_ACCEPTED");

			if (fu_hpi_cfu_send_offer_list_accepted(self) >= 0)
				priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER;
			else
				priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_OFFER");

			// send the same offer again
			reply = fu_hpi_cfu_send_offer_update_command(self, fw_offer);
			if (reply < 0)
				priv->state = FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR;
			else
				priv->state =
					FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED:
			// reply status must be SWAP_PENDING
			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED");
			reply = fu_hpi_cfu_firmware_update_offer_accepted(self);

			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED:reply:%d", reply);

			if (reply == FIRMWARE_UPDATE_OFFER_ACCEPT) {
				g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_OFFER_ACCEPTED: expceted a reject with SWAP PENDING");
				priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST;
				break;
			} else {
				if (fu_hpi_cfu_firmware_update_offer_rejected(reply)) {
					g_debug("CFU STATE: CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT: reply: %d,  OFFER_REJECTED: Reason: '%s'", reply, fu_cfu_rr_code_to_string(reason));

					switch (reason) {
					case FIRMWARE_OFFER_REJECT_OLD_FW:
						g_debug("CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT:FIRMWARE_OFFER_REJECT_OLD_FW: expcetd a reject with SWAP PENDING");
						break;

					case FIRMWARE_OFFER_REJECT_INV_MCU:
						g_debug("CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_OFFER_OFFER_ACCEPT:FIRMWARE_OFFER_REJECT_INV_MCU: expcetd a reject with SWAP PENDING");
						break;

					case FIRMWARE_UPDATE_OFFER_SWAP_PENDING:
						g_debug("FIRMWARE_UPDATE_OFFER_SWAP_PENDING: FIRMWARE UPDATE COMPLETED.");
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
					} // swicth
				} // rejected
				priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST;
			}
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST:
			g_debug("CFU STATE: CFU_UPDATE_VERIFY_CHECK_SWAP_PENDING_SEND_UPDATE_END_OFFER_LIST");

			reply = fu_hpi_cfu_send_end_offer_list(self);
			priv->state = FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED;
			break;

		case FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED:
			g_debug("CFU STATE: FU_HPI_CFU_STATE_VERIFY_CHECK_SWAP_PENDING_UPDATE_END_OFFER_LIST_ACCEPTED");
			reply = fu_hpi_cfu_end_offer_list_accepted(self);
			priv->state = FU_HPI_CFU_STATE_WAIT_DEVICE_REBOOT;
			fu_progress_step_done(progress); /* send-payload */
			break;

		case FU_HPI_CFU_STATE_WAIT_DEVICE_REBOOT:
			g_debug("WAITING FOR DEVICE TO RESET..! PLEASE WAIT...!");
			priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
			priv->device_reset = TRUE;
			fu_progress_step_done(progress); /* restart */
			break;

		case FU_HPI_CFU_STATE_UPDATE_VERIFY_ERROR:
			g_debug("CFU STATE: CFU_FIRMWARE_UPDATE_VERIFY_ERROR.Error happened during after update verfication.");
			priv->state = FU_HPI_CFU_STATE_UPDATE_STOP;
			break;

		case FU_HPI_CFU_STATE_UPDATE_ALL_OFFER_REJECTED:
			// if(all_offer_rejected(cfu_obj))
			if (lastpacket)
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
			return FALSE;
			break;
		default:
			g_debug("CFU STATE: None of the CFU STATE match.\n");
			break_loop = 1;
			break;
		}

		if (break_loop)
		{
			break;
		}
	} // while

	/* success */
	return TRUE;
}

static gboolean
fu_hpi_cfu_device_setup(FuDevice *device, GError **error)
{
	g_autoptr(GError) error_local = NULL;

	// Header is 4 bytes.
	gint32 versionTableOffset = 4;

	// Component ID is 6th byte.
	gint32 componentIDOffset = 5;

	// Each component takes up 8 bytes.
	gint32 componentDataSize = 8;

	// componentIndex - refers, if having multiple offers.
	// If offers count is 3, componentIndex starts from 0.
	gint32 componentIndex = 0; // Hardcoded to zero, since multiple offers logic is in progress.

	gsize actual_length = 0;
	guint8 buf[60];
	g_autoptr(GBytes) device_version_response = NULL;

	FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
	FuHpiCfuDevicePrivate *priv = GET_PRIVATE(self);


	g_return_val_if_fail(FU_HPI_CFU_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* FuHidDevice->setup */
	if (!FU_DEVICE_CLASS(fu_hpi_cfu_device_parent_class)->setup(device, error))
		return FALSE;

	if (!fu_usb_device_control_transfer(FU_USB_DEVICE(self),
					    FU_USB_DIRECTION_DEVICE_TO_HOST,
					    FU_USB_REQUEST_TYPE_VENDOR,
					    FU_USB_RECIPIENT_DEVICE,
					    GET_REPORT,
					    FEATURE_REPORT_TYPE | VERSION_REPORT_ID,
					    priv->iface_number,
					    buf,
					    sizeof(buf),
					    &actual_length,
					    FU_HPI_CFU_DEVICE_TIMEOUT,
					    NULL,
					    &error_local)) {
		g_set_error(error,
				FWUPD_ERROR,
				FWUPD_ERROR_INVALID_DATA,
				"failed to do device setup: %s", error_local->message);
		return FALSE;
	}

	device_version_response = g_bytes_new(buf, actual_length);
	fu_dump_bytes(G_LOG_DOMAIN, "fu_hpi_cfu_device_setup: bytes received", device_version_response);

	g_debug("Device Version: %02X.%02X.%02X.%02X", buf[8], buf[7], buf[6], buf[5]);

	fu_device_set_version(device, g_strdup_printf("%02X.%02X.%02X.%02X", buf[8], buf[7], buf[6], buf[5]));

	priv->burst_acksize = (gint32) buf[versionTableOffset + componentIndex * componentDataSize + componentIDOffset];
	g_debug("fu_hpi_cfu_device_setup: burst_acksize=%d", priv->burst_acksize);


	/* success */
	return TRUE;
}

static gboolean
fu_hpi_cfu_device_probe(FuDevice *device, GError **error)
{
	FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
	guint16 release = fu_usb_device_get_release(FU_USB_DEVICE(self));
	fu_device_add_instance_u16(device, "REV", release);

	if (!fu_device_build_instance_id_full(FU_DEVICE(self),
					      FU_DEVICE_INSTANCE_FLAG_GENERIC|
					      FU_DEVICE_INSTANCE_FLAG_VISIBLE|
					      FU_DEVICE_INSTANCE_FLAG_QUIRKS,
					      NULL,
					      "USB",
					      "VID",
					      "PID",
					      "REV",
					      NULL)) {
		g_set_error(error,
				FWUPD_ERROR,
				FWUPD_ERROR_INVALID_DATA,
				"failed build instance id with REV");
		return FALSE;
	}

	/* success */
	return TRUE;
}


static void
fu_hpi_cfu_set_progress(FuDevice *self, FuProgress *progress)
{
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_set_percentage(progress, 0);
	fu_progress_add_flag(progress, FU_PROGRESS_FLAG_GUESSED);
	fu_progress_add_step(progress, FWUPD_STATUS_DECOMPRESSING, 11, "detach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 34, "write");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 3, "attach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 52, "reload");
}

static gboolean
fu_hpi_cfu_device_write_firmware(FuDevice *device,
				 FuFirmware *firmware,
				 FuProgress *progress,
				 FwupdInstallFlags flags,
				 GError **error)
{
	FuHpiCfuDevice *self = FU_HPI_CFU_DEVICE(device);
	g_autoptr(FuFirmware) firmware_archive = fu_archive_firmware_new();
	g_autoptr(FuFirmware) fw_offer = NULL;
	g_autoptr(FuFirmware) fw_payload = NULL;
	g_autoptr(GBytes) blob_offer = NULL;
	g_autoptr(GBytes) blob_payload = NULL;

	/* progress */
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_step(progress, FWUPD_STATUS_DECOMPRESSING, 0, "start-entire");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 0, "start-offer");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 0, "send-offer");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 40 , "send-payload");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 54, "restart");


	/* get both images */
	fw_offer = fu_archive_firmware_get_image_fnmatch(FU_ARCHIVE_FIRMWARE(firmware),
							 "*.offer.bin",
							 error);
	if (fw_offer == NULL)
		return FALSE;

	fw_payload = fu_archive_firmware_get_image_fnmatch(FU_ARCHIVE_FIRMWARE(firmware),
								"*.payload.bin",
								error);
	if (fw_payload == NULL)
		return FALSE;


	/* cfu state machine implementation */
	if (!fu_hpi_cfu_update_firmware_state_machine(self, progress, fw_offer, fw_payload)) {
		g_set_error(error,
				FWUPD_ERROR,
				FWUPD_ERROR_NOT_SUPPORTED,
				"failed firmware_state_machine to perform firmware update.");
		return FALSE;
	}

	/* the device automatically reboots */
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);	

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
	fu_device_add_internal_flag(FU_DEVICE(self), FU_DEVICE_INTERNAL_FLAG_ADD_INSTANCE_ID_REV);

	/* The HP Dock device reboot takes down the entire hub for ~8 minutes */
	fu_device_set_remove_delay(FU_DEVICE(self), 480 * 1000);
}

static void
fu_hpi_cfu_device_class_init(FuHpiCfuDeviceClass *klass)
{
	FuDeviceClass *device_class = FU_DEVICE_CLASS(klass);

	device_class->write_firmware = fu_hpi_cfu_device_write_firmware;
	device_class->probe = fu_hpi_cfu_device_probe;
	device_class->setup = fu_hpi_cfu_device_setup;
	device_class->set_progress = fu_hpi_cfu_set_progress;
}
