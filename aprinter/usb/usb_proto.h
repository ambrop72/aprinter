/*
 * Copyright (c) 2013 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AMBROLIB_USB_PROTO_H
#define AMBROLIB_USB_PROTO_H

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

enum {
    USB_DESSCRIPTOR_TYPE_DEVICE = 1,
    USB_DESSCRIPTOR_TYPE_CONFIGURATION = 2,
    USB_DESSCRIPTOR_TYPE_STRING = 3,
    USB_DESSCRIPTOR_TYPE_INTERFACE = 4,
    USB_DESSCRIPTOR_TYPE_ENDPOINT = 5
};

enum {
    USB_CONFIGURATION_ATTRIBUTE_RESERVED7 = (1 << 7),
    USB_CONFIGURATION_ATTRIBUTE_SELF_POWERED = (1 << 6),
    USB_CONFIGURATION_ATTRIBUTE_REMOTE_WAKEUP = (1 << 5)
};

enum {
    USB_ENDPOINT_ADDRESS_DIRECTION = (1 << 7)
};

enum {
    USB_TRANSFER_TYPE_CONTROL = 0,
    USB_TRANSFER_TYPE_ISOCHRONOUS = 1,
    USB_TRANSFER_TYPE_BULK = 2,
    USB_TRANSFER_TYPE_INTERRUPT = 3,
};

enum {
    USB_INTERFACE_ATTRIBUTES_OFFSET_TRANSFER_TYPE = 0,
    USB_INTERFACE_ATTRIBUTES_OFFSET_SYNC_TYPE = 2,
    USB_INTERFACE_ATTRIBUTES_OFFSET_USAGE_TYPE = 4
};

enum {
    USB_INTERFACE_ATTRIBUTES_MASK_TRANSFER_TYPE = 3,
    USB_INTERFACE_ATTRIBUTES_MASK_SYNC_TYPE = 3,
    USB_INTERFACE_ATTRIBUTES_MASK_USAGE_TYPE = 3
};

enum {
    USB_REQUEST_TYPE_OFFSET_DPTD = 7,
    USB_REQUEST_TYPE_OFFSET_TYPE = 5,
    USB_REQUEST_TYPE_OFFSET_RECIPIENT = 0
};

enum {
    USB_REQUEST_TYPE_MASK_DPTD = 1,
    USB_REQUEST_TYPE_MASK_TYPE = 3,
    USB_REQUEST_TYPE_MASK_RECIPIENT = 31
};

enum {
    USB_REQUEST_TYPE_DPTD_HOST_TO_DEVICE = 0,
    USB_REQUEST_TYPE_DPTD_DEVICE_TO_HOST = 1
};

enum {
    USB_REQUEST_TYPE_TYPE_STANDARD = 0,
    USB_REQUEST_TYPE_TYPE_CLASS = 1,
    USB_REQUEST_TYPE_TYPE_VENDOR = 2,
    USB_REQUEST_TYPE_TYPE_RESERVED = 3
};

enum {
    USB_REQUEST_TYPE_RECIPIENT_DEVICE = 0,
    USB_REQUEST_TYPE_RECIPIENT_INTERFACE = 1,
    USB_REQUEST_TYPE_RECIPIENT_ENDPOINT = 2,
    USB_REQUEST_TYPE_RECIPIENT_OTHER = 3
};

enum {
    USB_REQUEST_TYPE_H2D_STD_DEV = 0x00,
    USB_REQUEST_TYPE_D2H_STD_DEV = 0x80
};

enum {
    USB_REQUEST_ID_GET_STATUS = 0,
    USB_REQUEST_ID_CLEAR_FEATURE = 1,
    USB_REQUEST_ID_SET_FEATURE = 3,
    USB_REQUEST_ID_SET_ADDRESS = 5,
    USB_REQUEST_ID_GET_DESCRIPTOR = 6,
    USB_REQUEST_ID_SET_DESCRIPTOR = 7,
    USB_REQUEST_ID_GET_CONFIGURATION = 8,
    USB_REQUEST_ID_GET_SET_CONFIGURATION = 9,
    USB_REQUEST_ID_GET_INTERFACE = 0xA,
    USB_REQUEST_ID_SET_INTERFACE = 0x11,
    USB_REQUEST_ID_SYNCH_FRAME = 0x12
};

struct UsbDeviceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB ;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed));

struct UsbConfigurationDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t mbAttributes;
    uint8_t bMaxPower;
} __attribute__((packed));

struct UsbInterfaceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));

struct UsbEndpointDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed));

struct UsbSetupPacket {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

#include <aprinter/EndNamespace.h>

#endif
