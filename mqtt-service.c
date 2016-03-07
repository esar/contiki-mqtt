/*
 * Copyright (c) 2014, Stephen Robinson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 *  are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "contiki.h"
#include "contiki-net.h"
#include "sys/etimer.h"

#include <string.h>
#include <stdio.h>

#include "mqtt-service.h"


typedef struct mqtt_state_t
{
  uip_ip6addr_t address;
  uint16_t port;
  int auto_reconnect;
  mqtt_connect_info_t* connect_info;

  struct process* calling_process;
  struct uip_conn* tcp_connection;

  struct psock ps;
  uint8_t* in_buffer;
  uint8_t* out_buffer;
  int in_buffer_length;
  int out_buffer_length;
  uint16_t message_length;
  uint16_t message_length_read;
  mqtt_message_t* outbound_message;
  mqtt_connection_t mqtt_connection;
  uint16_t pending_msg_id;
  int pending_msg_type;

} mqtt_state_t;

PROCESS(mqtt_process, "MQTT Process");

process_event_t mqtt_event;
mqtt_state_t mqtt_state;
int mqtt_flags = 0;


/*********************************************************************
*
*    Public API
*
*********************************************************************/

// Initialise the MQTT client, must be called before anything else
void mqtt_init(uint8_t* in_buffer, int in_buffer_length, uint8_t* out_buffer, int out_buffer_length)
{
  mqtt_event = process_alloc_event();

  mqtt_state.in_buffer = in_buffer;
  mqtt_state.in_buffer_length = in_buffer_length;
  mqtt_state.out_buffer = out_buffer;
  mqtt_state.out_buffer_length = out_buffer_length;
}

// Connect to the specified server
int mqtt_connect(uip_ip6addr_t* address, uint16_t port, int auto_reconnect, mqtt_connect_info_t* info)
{
  if(process_is_running(&mqtt_process))
    return -1;

  mqtt_state.address = *address;
  mqtt_state.port = port;
  mqtt_state.auto_reconnect = auto_reconnect;
  mqtt_state.connect_info = info;
  mqtt_state.calling_process = PROCESS_CURRENT();
  process_start(&mqtt_process, (void*)&mqtt_state);

  return 0;
}

// Disconnect from the server
int mqtt_disconnect()
{
  if(!process_is_running(&mqtt_process))
    return -1;

  printf("mqtt: exiting...\n");
  mqtt_flags &= ~MQTT_FLAG_READY;
  mqtt_flags |= MQTT_FLAG_EXIT;
  tcpip_poll_tcp(mqtt_state.tcp_connection);

  return 0;
}

// Subscribe to the specified topic
int mqtt_subscribe(const char* topic)
{
  if(!mqtt_ready())
    return -1;

  printf("mqtt: sending subscribe...\n");
  mqtt_state.outbound_message = mqtt_msg_subscribe(&mqtt_state.mqtt_connection, 
                                                   topic, 0, 
                                                   &mqtt_state.pending_msg_id);
  mqtt_flags &= ~MQTT_FLAG_READY;
  mqtt_state.pending_msg_type = MQTT_MSG_TYPE_SUBSCRIBE;
  tcpip_poll_tcp(mqtt_state.tcp_connection);

  return 0;
}

int mqtt_unsubscribe(const char* topic)
{
  if(!mqtt_ready())
    return -1;

  printf("sending unsubscribe\n");
  mqtt_state.outbound_message = mqtt_msg_unsubscribe(&mqtt_state.mqtt_connection, topic, 
                                                     &mqtt_state.pending_msg_id);
  mqtt_flags &= ~MQTT_FLAG_READY;
  mqtt_state.pending_msg_type = MQTT_MSG_TYPE_UNSUBSCRIBE;
  tcpip_poll_tcp(mqtt_state.tcp_connection);

  return 0;
}

// Publish the specified message
int mqtt_publish_with_length(const char* topic, const char* data, int data_length, int qos, int retain)
{
  if(!mqtt_ready())
    return -1;

  printf("mqtt: sending publish...\n");
  mqtt_state.outbound_message = mqtt_msg_publish(&mqtt_state.mqtt_connection, 
                                                 topic, data, data_length, 
                                                 qos, retain,
                                                 &mqtt_state.pending_msg_id);
  mqtt_flags &= ~MQTT_FLAG_READY;
  mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PUBLISH;
  tcpip_poll_tcp(mqtt_state.tcp_connection);

  return 0;
}


/***************************************************************
*
*    Internals
*
***************************************************************/

static void complete_pending(mqtt_state_t* state, int event_type)
{
  mqtt_event_data_t event_data;

  state->pending_msg_type = 0;
  mqtt_flags |= MQTT_FLAG_READY;
  event_data.type = event_type;
  process_post_synch(state->calling_process, mqtt_event, &event_data);
}

static void deliver_publish(mqtt_state_t* state, uint8_t* message, int length)
{
  mqtt_event_data_t event_data;

  event_data.type = MQTT_EVENT_TYPE_PUBLISH;

  event_data.topic_length = length;
  event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);

  event_data.data_length = length;
  event_data.data = mqtt_get_publish_data(message, &event_data.data_length);

  memmove((char*)event_data.data + 1, (char*)event_data.data, event_data.data_length);
  event_data.data += 1;
  ((char*)event_data.topic)[event_data.topic_length] = '\0';
  ((char*)event_data.data)[event_data.data_length] = '\0';

  process_post_synch(state->calling_process, mqtt_event, &event_data);
}

static void deliver_publish_continuation(mqtt_state_t* state, uint16_t offset, uint8_t* buffer, uint16_t length)
{
  mqtt_event_data_t event_data;

  event_data.type = MQTT_EVENT_TYPE_PUBLISH_CONTINUATION;
  event_data.topic_length = 0;
  event_data.topic = NULL;
  event_data.data_length = length;
  event_data.data_offset = offset;
  event_data.data = (char*)buffer;
  ((char*)event_data.data)[event_data.data_length] = '\0';

  process_post_synch(state->calling_process, mqtt_event, &event_data);
}

static PT_THREAD(handle_mqtt_connection(mqtt_state_t* state))
{
  static struct etimer keepalive_timer;

  uint8_t msg_type;
  uint8_t msg_qos;
  uint16_t msg_id;

  PSOCK_BEGIN(&state->ps);

  // Initialise and send CONNECT message
  mqtt_msg_init(&state->mqtt_connection, state->out_buffer, state->out_buffer_length);
  state->outbound_message =  mqtt_msg_connect(&state->mqtt_connection, state->connect_info);
  PSOCK_SEND(&state->ps, state->outbound_message->data, state->outbound_message->length);
  state->outbound_message = NULL;

  // Wait for CONACK message
  PSOCK_READBUF_LEN(&state->ps, 2);
  if(mqtt_get_type(state->in_buffer) != MQTT_MSG_TYPE_CONNACK)
    PSOCK_CLOSE_EXIT(&state->ps);
  
  // Tell the client we're connected
  mqtt_flags |= MQTT_FLAG_CONNECTED;
  complete_pending(state, MQTT_EVENT_TYPE_CONNECTED);

  // Setup the keep alive timer and enter main message processing loop
  etimer_set(&keepalive_timer, CLOCK_SECOND * state->connect_info->keepalive);
  while(1)
  {
    // Wait for something to happen: 
    //   new incoming data, 
    //   new outgoing data, 
    //   keep alive timer expired
    PSOCK_WAIT_UNTIL(&state->ps, PSOCK_NEWDATA(&state->ps) || 
                                 state->outbound_message != NULL ||
                                 etimer_expired(&keepalive_timer));

    // If there's a new message waiting to go out, then send it
    if(state->outbound_message != NULL)
    {
      PSOCK_SEND(&state->ps, state->outbound_message->data, state->outbound_message->length);
      state->outbound_message = NULL;

      // If it was a PUBLISH message with QoS-0 then tell the client it's done
      if(state->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && state->pending_msg_id == 0)
        complete_pending(state, MQTT_EVENT_TYPE_PUBLISHED);

      // Reset the keepalive timer as we've just sent some data
      etimer_restart(&keepalive_timer);
      continue;
    }

    // If the keep-alive timer expired then prepare a ping for sending
    // and reset the timer
    if(etimer_expired(&keepalive_timer))
    {
      state->outbound_message = mqtt_msg_pingreq(&state->mqtt_connection);
      etimer_reset(&keepalive_timer);
      continue;
    }

    // If we get here we must have woken for new incoming data, 
    // read and process it.
    PSOCK_READBUF_LEN(&state->ps, 2);
    
    state->message_length_read = PSOCK_DATALEN(&state->ps);
    state->message_length = mqtt_get_total_length(state->in_buffer, state->message_length_read);

    msg_type = mqtt_get_type(state->in_buffer);
    msg_qos  = mqtt_get_qos(state->in_buffer);
    msg_id   = mqtt_get_id(state->in_buffer, state->in_buffer_length);
    switch(msg_type)
    {
      case MQTT_MSG_TYPE_SUBACK:
        if(state->pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && state->pending_msg_id == msg_id)
          complete_pending(state, MQTT_EVENT_TYPE_SUBSCRIBED);
        break;
      case MQTT_MSG_TYPE_UNSUBACK:
        if(state->pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && state->pending_msg_id == msg_id)
          complete_pending(state, MQTT_EVENT_TYPE_UNSUBSCRIBED);
        break;
      case MQTT_MSG_TYPE_PUBLISH:
        if(msg_qos == 1)
          state->outbound_message = mqtt_msg_puback(&state->mqtt_connection, msg_id);
        else if(msg_qos == 2)
          state->outbound_message = mqtt_msg_pubrec(&state->mqtt_connection, msg_id);

        deliver_publish(state, state->in_buffer, state->message_length_read);
        break;
      case MQTT_MSG_TYPE_PUBACK:
        if(state->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && state->pending_msg_id == msg_id)
          complete_pending(state, MQTT_EVENT_TYPE_PUBLISHED);
        break;
      case MQTT_MSG_TYPE_PUBREC:
        state->outbound_message = mqtt_msg_pubrel(&state->mqtt_connection, msg_id);
        break;
      case MQTT_MSG_TYPE_PUBREL:
        state->outbound_message = mqtt_msg_pubcomp(&state->mqtt_connection, msg_id);
        break;
      case MQTT_MSG_TYPE_PUBCOMP:
        if(state->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && state->pending_msg_id == msg_id)
          complete_pending(state, MQTT_EVENT_TYPE_PUBLISHED);
        break;
      case MQTT_MSG_TYPE_PINGREQ:
        state->outbound_message = mqtt_msg_pingresp(&state->mqtt_connection);
        break;
      case MQTT_MSG_TYPE_PINGRESP:
        // Ignore
        break;
    }

    // NOTE: this is done down here and not in the switch case above
    //       because the PSOCK_READBUF_LEN() won't work inside a switch
    //       statement due to the way protothreads resume.
    if(msg_type == MQTT_MSG_TYPE_PUBLISH)
    {
      uint16_t len;

      // adjust message_length and message_length_read so that
      // they only account for the publish data and not the rest of the 
      // message, this is done so that the offset passed with the
      // continuation event is the offset within the publish data and
      // not the offset within the message as a whole.
      len = state->message_length_read;
      mqtt_get_publish_data(state->in_buffer, &len);
      len = state->message_length_read - len;
      state->message_length -= len;
      state->message_length_read -= len;

      while(state->message_length_read < state->message_length)
      {
        PSOCK_READBUF_LEN(&state->ps, state->message_length - state->message_length_read);
        deliver_publish_continuation(state, state->message_length_read, state->in_buffer, PSOCK_DATALEN(&state->ps));
        state->message_length_read += PSOCK_DATALEN(&state->ps);
      }
    }
  }

  PSOCK_END(&state->ps);
}

PROCESS_THREAD(mqtt_process, ev, data)
{
  mqtt_event_data_t event_data;

  PROCESS_BEGIN();

  while(1)
  {
    printf("mqtt: connecting...\n");
    mqtt_state.tcp_connection = tcp_connect(&mqtt_state.address, 
                                            mqtt_state.port, NULL);
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);

    if(!uip_connected())
    {
      printf("mqtt: connect failed\n");
      continue;
    }
    else
      printf("mqtt: connected\n");

    // reserve one byte at the end of the buffer so there's space to NULL terminate
    PSOCK_INIT(&mqtt_state.ps, mqtt_state.in_buffer, mqtt_state.in_buffer_length - 1);

    handle_mqtt_connection(&mqtt_state);

    while(1)
    {
      PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);

      if(mqtt_flags & MQTT_FLAG_EXIT)
      {
        uip_close();

        event_data.type = MQTT_EVENT_TYPE_EXITED;
        process_post_synch(mqtt_state.calling_process, mqtt_event, &event_data);
        PROCESS_EXIT();
      }

      if(uip_aborted() || uip_timedout() || uip_closed())
      {
        event_data.type = MQTT_EVENT_TYPE_DISCONNECTED;
        process_post_synch(mqtt_state.calling_process, mqtt_event, &event_data);
        printf("mqtt: lost connection: %s\n", uip_aborted() ? "aborted" : 
                                              uip_timedout() ? "timed out" : 
                                              uip_closed() ? "closed" : 
                                              "unknown");
        break;
      }
      else
        handle_mqtt_connection(&mqtt_state);
    }

    if(!mqtt_state.auto_reconnect)
      break;
  }

  event_data.type = MQTT_EVENT_TYPE_EXITED;
  process_post_synch(mqtt_state.calling_process, mqtt_event, &event_data);

  PROCESS_END();
}
