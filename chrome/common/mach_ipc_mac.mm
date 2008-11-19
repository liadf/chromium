// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <stdio.h>
#import "chrome/common/mach_ipc_mac.h"

//==============================================================================
MachSendMessage::MachSendMessage(int32_t message_id) : MachMessage() {
  head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);

  // head.msgh_remote_port = ...; // filled out in MachPortSender::SendMessage()
  head.msgh_local_port = MACH_PORT_NULL;
  head.msgh_reserved = 0;
  head.msgh_id = 0;

  SetDescriptorCount(0);  // start out with no descriptors

  SetMessageID(message_id);
  SetData(NULL, 0);       // client may add data later
}

//==============================================================================
// returns true if successful
bool MachMessage::SetData(void *data,
                          int32_t data_length) {
  // first check to make sure we have enough space
  int size = CalculateSize();
  int new_size = size + data_length;
  
  if ((unsigned)new_size > sizeof(MachMessage)) {
    return false;  // not enough space
  }

  GetDataPacket()->data_length = EndianU32_NtoL(data_length);
  if (data) memcpy(GetDataPacket()->data, data, data_length);

  CalculateSize();

  return true;
}

//==============================================================================
// calculates and returns the total size of the message
// Currently, the entire message MUST fit inside of the MachMessage
//    messsage size <= sizeof(MachMessage)
int MachMessage::CalculateSize() {
  int size = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t);
  
  // add space for MessageDataPacket
  int32_t alignedDataLength = (GetDataLength() + 3) & ~0x3;
  size += 2*sizeof(int32_t) + alignedDataLength;
  
  // add space for descriptors
  size += GetDescriptorCount() * sizeof(MachMsgPortDescriptor);
  
  head.msgh_size = size;
  
  return size;
}

//==============================================================================
MachMessage::MessageDataPacket *MachMessage::GetDataPacket() {
  int desc_size = sizeof(MachMsgPortDescriptor)*GetDescriptorCount();
  MessageDataPacket *packet =
    reinterpret_cast<MessageDataPacket*>(padding + desc_size);

  return packet;
}

//==============================================================================
void MachMessage::SetDescriptor(int n,
                                const MachMsgPortDescriptor &desc) {
  MachMsgPortDescriptor *desc_array =
    reinterpret_cast<MachMsgPortDescriptor*>(padding);
  desc_array[n] = desc;
}

//==============================================================================
// returns true if successful otherwise there was not enough space
bool MachMessage::AddDescriptor(const MachMsgPortDescriptor &desc) {
  // first check to make sure we have enough space
  int size = CalculateSize();
  int new_size = size + sizeof(MachMsgPortDescriptor);
  
  if ((unsigned)new_size > sizeof(MachMessage)) {
    return false;  // not enough space
  }

  // unfortunately, we need to move the data to allow space for the
  // new descriptor
  u_int8_t *p = reinterpret_cast<u_int8_t*>(GetDataPacket());
  bcopy(p, p+sizeof(MachMsgPortDescriptor), GetDataLength()+2*sizeof(int32_t));
  
  SetDescriptor(GetDescriptorCount(), desc);
  SetDescriptorCount(GetDescriptorCount() + 1);

  CalculateSize();
  
  return true;
}

//==============================================================================
void MachMessage::SetDescriptorCount(int n) {
  body.msgh_descriptor_count = n;

  if (n > 0) {
    head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
  } else {
    head.msgh_bits &= ~MACH_MSGH_BITS_COMPLEX;
  }
}

//==============================================================================
MachMsgPortDescriptor *MachMessage::GetDescriptor(int n) {
  if (n < GetDescriptorCount()) {
    MachMsgPortDescriptor *desc =
      reinterpret_cast<MachMsgPortDescriptor*>(padding);
    return desc + n;
  }
  
  return nil;
}

//==============================================================================
mach_port_t MachMessage::GetTranslatedPort(int n) {
  if (n < GetDescriptorCount()) {
    return GetDescriptor(n)->GetMachPort();
  }
  return MACH_PORT_NULL;
}

#pragma mark -

//==============================================================================
// create a new mach port for receiving messages and register a name for it
ReceivePort::ReceivePort(const char *receive_port_name) {
  mach_port_t current_task = mach_task_self();

  init_result_ = mach_port_allocate(current_task,
                                    MACH_PORT_RIGHT_RECEIVE,
                                    &port_);

  if (init_result_ != KERN_SUCCESS)
    return;
    
  init_result_ = mach_port_insert_right(current_task,
                                        port_,
                                        port_,
                                        MACH_MSG_TYPE_MAKE_SEND);

  if (init_result_ != KERN_SUCCESS)
    return;

  mach_port_t bootstrap_port = 0;
  init_result_ = task_get_bootstrap_port(current_task, &bootstrap_port);

  if (init_result_ != KERN_SUCCESS)
    return;

  init_result_ = bootstrap_register(bootstrap_port,
                                    const_cast<char*>(receive_port_name),
                                    port_);
}

//==============================================================================
// create a new mach port for receiving messages
ReceivePort::ReceivePort() {
  mach_port_t current_task = mach_task_self();

  init_result_ = mach_port_allocate(current_task,
                                    MACH_PORT_RIGHT_RECEIVE,
                                    &port_);

  if (init_result_ != KERN_SUCCESS)
    return;

  init_result_ =   mach_port_insert_right(current_task,
                                          port_,
                                          port_,
                                          MACH_MSG_TYPE_MAKE_SEND);
}

//==============================================================================
// Given an already existing mach port, use it.  We take ownership of the
// port and deallocate it in our destructor.
ReceivePort::ReceivePort(mach_port_t receive_port)
  : port_(receive_port),
    init_result_(KERN_SUCCESS) {
}

//==============================================================================
ReceivePort::~ReceivePort() {
  if (init_result_ == KERN_SUCCESS)
    mach_port_deallocate(mach_task_self(), port_);
}

//==============================================================================
kern_return_t ReceivePort::WaitForMessage(MachReceiveMessage *out_message,
                                          mach_msg_timeout_t timeout) {
  if (!out_message) {
    return KERN_INVALID_ARGUMENT;
  }

  // return any error condition encountered in constructor
  if (init_result_ != KERN_SUCCESS)
    return init_result_;
  
  out_message->head.msgh_bits = 0;
  out_message->head.msgh_local_port = port_;
  out_message->head.msgh_remote_port = MACH_PORT_NULL;
  out_message->head.msgh_reserved = 0;
  out_message->head.msgh_id = 0;

  kern_return_t result = mach_msg(&out_message->head,
                                  MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                                  0,
                                  sizeof(MachMessage),
                                  port_,
                                  timeout,              // timeout in ms
                                  MACH_PORT_NULL);

  return result;
}

#pragma mark -

//==============================================================================
// get a port with send rights corresponding to a named registered service
MachPortSender::MachPortSender(const char *receive_port_name) {
  mach_port_t bootstrap_port = 0;
  init_result_ = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
  
  if (init_result_ != KERN_SUCCESS)
    return;

  init_result_ = bootstrap_look_up(bootstrap_port,
                    const_cast<char*>(receive_port_name),
                    &send_port_);
}

//==============================================================================
MachPortSender::MachPortSender(mach_port_t send_port) 
  : send_port_(send_port),
    init_result_(KERN_SUCCESS) {
}

//==============================================================================
kern_return_t MachPortSender::SendMessage(MachSendMessage &message,
                                          mach_msg_timeout_t timeout) {
  if (message.head.msgh_size == 0) {
    return KERN_INVALID_VALUE;    // just for safety -- never should occur
  };
  
  if (init_result_ != KERN_SUCCESS)
    return init_result_;
  
  message.head.msgh_remote_port = send_port_;

  kern_return_t result = mach_msg(&message.head,
                                  MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                                  message.head.msgh_size,
                                  0,
                                  MACH_PORT_NULL,
                                  timeout,              // timeout in ms
                                  MACH_PORT_NULL);

  return result;
}
