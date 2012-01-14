/*
 * Copyright (c) 2011-2012, Nicholas Humfrey.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef NanodeMQTT_h
#define NanodeMQTT_h

#include <NanodeUIP.h>


// Uncomment to enable debugging of NanodeMQTT
//#define MQTT_DEBUG   1


#define MQTT_DEFAULT_PORT        (1883)
#define MQTT_DEFAULT_KEEP_ALIVE  (15)

#define MQTT_MAX_CLIENT_ID_LEN   (23)
#define MQTT_MAX_PAYLOAD_LEN     (127)


// MQTT Packet Types
#define MQTT_TYPE_CONNECT     (1 << 4)  // Client request to connect to Server
#define MQTT_TYPE_CONNACK     (2 << 4)  // Connect Acknowledgment
#define MQTT_TYPE_PUBLISH     (3 << 4)  // Publish message
#define MQTT_TYPE_TYPE_PUBACK (4 << 4)  // Publish Acknowledgment
#define MQTT_TYPE_PUBREC      (5 << 4)  // Publish Received (assured delivery part 1)
#define MQTT_TYPE_PUBREL      (6 << 4)  // Publish Release (assured delivery part 2)
#define MQTT_TYPE_PUBCOMP     (7 << 4)  // Publish Complete (assured delivery part 3)
#define MQTT_TYPE_SUBSCRIBE   (8 << 4)  // Client Subscribe request
#define MQTT_TYPE_SUBACK      (9 << 4)  // Subscribe Acknowledgment
#define MQTT_TYPE_UNSUBSCRIBE (10 << 4) // Client Unsubscribe request
#define MQTT_TYPE_UNSUBACK    (11 << 4) // Unsubscribe Acknowledgment
#define MQTT_TYPE_PINGREQ     (12 << 4) // PING Request
#define MQTT_TYPE_PINGRESP    (13 << 4) // PING Response
#define MQTT_TYPE_DISCONNECT  (14 << 4) // Client is Disconnecting

// Fixed header flags (second nibble)
#define MQTT_FLAG_DUP         (0x08)
#define MQTT_FLAG_QOS_0       (0x00)
#define MQTT_FLAG_QOS_1       (0x02)
#define MQTT_FLAG_QOS_2       (0x04)
#define MQTT_FLAG_RETAIN      (0x01)

// CONNECT header flags
#define MQTT_PROTOCOL_VERSION (3)
#define MQTT_FLAG_USERNAME    (0x80)
#define MQTT_FLAG_PASSWORD    (0x40)
#define MQTT_FLAG_WILL_RETAIN (0x20)
#define MQTT_FLAG_WILL_QOS_0  (0x00)
#define MQTT_FLAG_WILL_QOS_1  (0x08)
#define MQTT_FLAG_WILL_QOS_2  (0x10)
#define MQTT_FLAG_WILL        (0x04)
#define MQTT_FLAG_CLEAN       (0x02)


enum mqtt_state {
  MQTT_STATE_WAITING,        // Waiting for TCP to connect
  MQTT_STATE_CONNECTING,     // TCP Connected and in middle of sending a CONNECT
  MQTT_STATE_CONNECT_SENT,   // Waiting for CONNACK
  MQTT_STATE_CONNECTED,      // Received CONNACK
  MQTT_STATE_PUBLISHING,     // In the middle of sending a PUBLISH
  MQTT_STATE_SUBSCRIBING,    // In the middle of sending a SUBSCRIBE
  MQTT_STATE_SUBSCRIBE_SENT, // Waiting for a SUBACK
  MQTT_STATE_PINGING,        // In the middle of sending a PING
  MQTT_STATE_DISCONNECTING,  // In the middle of sending a DISCONNECT packet
  MQTT_STATE_DISCONNECTED
};


#ifdef MQTT_DEBUG
#define MQTT_DEBUG_PRINTLN(str) printf_P(PSTR(str "\n"));
#define MQTT_DEBUG_PRINTF(str, ...) printf_P(PSTR(str), __VA_ARGS__);
#else
#define MQTT_DEBUG_PRINTLN(str)
#define MQTT_DEBUG_PRINTF(str, ...)
#endif


typedef void (*mqtt_callback_t) (const char* topic, uint8_t* payload, int payload_length);



class NanodeMQTT {
private:
  NanodeUIP *uip;
  char client_id[MQTT_MAX_CLIENT_ID_LEN+1];
  uip_ipaddr_t addr;
  uint16_t port;
  uint16_t keep_alive;
  uint16_t message_id;
  uint8_t state;
  uint8_t ping_pending;
  uint8_t blocking_mode;
  int8_t error_code;

  uint8_t *buf;
  uint8_t pos;

  struct timer receive_timer;
  struct timer transmit_timer;

  // Publishing
  // FIXME: can we do without these buffers
  char payload_topic[32];
  uint8_t payload[MQTT_MAX_PAYLOAD_LEN];
  uint8_t payload_length;
  uint8_t payload_retain;

  // Subscribing
  const char *subscribe_topic;
  mqtt_callback_t callback;

public:
  NanodeMQTT(NanodeUIP *uip);

  void set_client_id(const char* client_id);
  void set_server_addr(byte a, byte b, byte c, byte d);
  void set_server_port(uint16_t port);
  void set_keep_alive(uint16_t secs);
  void set_blocking_mode(uint8_t blocking);
  void set_callback(mqtt_callback_t callback);

  void connect();
  void disconnect();
  uint8_t connected();
  uint8_t get_state();
  int8_t get_error_code();

  void publish(const char* topic, const char* payload);
  void publish(const char* topic, const uint8_t* payload, uint8_t plength);
  void publish(const char* topic, const uint8_t* payload, uint8_t plength, uint8_t retained);

  void subscribe(const char* topic);



  // uIP Callbacks (used internally)
  void tcp_closed();
  void tcp_connected();
  void tcp_acked();
  void tcp_receive();
  void tcp_transmit();
  void poll();
  void check_timeout();


private:
  // Packet assembly functions
  void init_packet(uint8_t header);
  void append_byte(uint8_t b);
  void append_word(uint16_t s);
  void append_string(const char* str);
  void append_data(uint8_t *data, uint8_t data_len);
  void send_packet();
};


struct mqtt_app_state {
  NanodeMQTT* mqtt;
};
UIPASSERT(sizeof(struct mqtt_app_state)<=TCP_APP_STATE_SIZE)


#endif
