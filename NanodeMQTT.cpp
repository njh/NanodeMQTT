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

#include <NanodeMQTT.h>


#ifdef MQTT_DEBUG
static int serial_putc( char c, FILE * )
{
  Serial.write( c );
  return c;
}
#endif

NanodeMQTT::NanodeMQTT(NanodeUIP *uip)
{
  this->uip = uip;
  memset(this->client_id, 0, MQTT_MAX_CLIENT_ID_LEN+1);
  this->port = MQTT_DEFAULT_PORT;
  this->keep_alive = MQTT_DEFAULT_KEEP_ALIVE;
  this->message_id = 0;
  this->state = MQTT_STATE_DISCONNECTED;
  this->ping_pending = 0;
  this->blocking_mode = 1;
  this->error_code = 0;

  this->payload_length = 0;
  this->payload_retain = 0;
  this->subscribe_topic = NULL;
  this->callback = NULL;

  #ifdef MQTT_DEBUG
  fdevopen( &serial_putc, 0 );
  #endif
}

static void mqtt_appcall(void)
{
  NanodeMQTT *mqtt = ((struct mqtt_app_state *)&(uip_conn->appstate))->mqtt;

  if (uip_aborted()) {
    MQTT_DEBUG_PRINTLN("TCP: Connection Aborted");
    mqtt->tcp_closed();
  }

  if (uip_timedout()) {
    MQTT_DEBUG_PRINTLN("TCP: Connection Timed Out");
    mqtt->tcp_closed();
  }

  if (uip_closed()) {
    MQTT_DEBUG_PRINTLN("TCP: Connection Closed");
    mqtt->tcp_closed();
  }

  if (uip_connected()) {
    MQTT_DEBUG_PRINTLN("TCP: Connection Established");
    mqtt->tcp_connected();
  }

  if (uip_acked()) {
    MQTT_DEBUG_PRINTLN("TCP: Acked");
    mqtt->tcp_acked();
  }

  if (uip_newdata()) {
    MQTT_DEBUG_PRINTLN("TCP: New Data");
    mqtt->tcp_receive();
  }

  if (uip_rexmit()) {
    MQTT_DEBUG_PRINTLN("TCP: Retransmit");
    mqtt->tcp_transmit();
  }

  if (uip_poll()) {
    mqtt->poll();
  }

  mqtt->check_timeout();
}


void NanodeMQTT::set_client_id(const char* client_id)
{
  strncpy(this->client_id, client_id, MQTT_MAX_CLIENT_ID_LEN);
}

void NanodeMQTT::set_server_addr(byte a, byte b, byte c, byte d)
{
  uip_ipaddr(&this->addr, a,b,c,d);
}

void NanodeMQTT::set_server_port(uint16_t port)
{
  this->port = port;
}

void NanodeMQTT::set_keep_alive(uint16_t secs)
{
  this->keep_alive = secs;
}

void NanodeMQTT::set_blocking_mode(uint8_t blocking)
{
  this->blocking_mode = blocking;
}

void NanodeMQTT::set_callback(mqtt_callback_t callback)
{
  this->callback = callback;
}


// FIXME: add support for WILL
void NanodeMQTT::connect()
{
  struct uip_conn *conn;

  // Set the client ID to the MAC address, if none is set
  if (this->client_id[0] == '\0') {
    uip->get_mac_str(this->client_id);
  }

  conn = uip_connect(&this->addr, UIP_HTONS(this->port), mqtt_appcall);
  if (conn) {
    struct mqtt_app_state *s = (struct mqtt_app_state *)&(conn->appstate);
    s->mqtt = this;
    this->state = MQTT_STATE_WAITING;

    // Set the keep-alive timers
    timer_set(&this->transmit_timer, CLOCK_SECOND * this->keep_alive);
    timer_set(&this->receive_timer, CLOCK_SECOND * this->keep_alive);

    // If in blocking mode - loop until we are connected
    if (this->blocking_mode) {
      while (this->state == MQTT_STATE_WAITING ||
             this->state == MQTT_STATE_CONNECTING ||
             this->state == MQTT_STATE_CONNECT_SENT)
      {
        uip->poll();
      }
    }
  }
}

uint8_t NanodeMQTT::connected()
{
  switch(this->state) {
    case MQTT_STATE_CONNECTED:
    case MQTT_STATE_PUBLISHING:
    case MQTT_STATE_SUBSCRIBING:
    case MQTT_STATE_SUBSCRIBE_SENT:
    case MQTT_STATE_PINGING:
      return 1;
    default:
      return 0;
  }
}

uint8_t NanodeMQTT::get_state()
{
  return this->state;
}

int8_t NanodeMQTT::get_error_code()
{
  return this->error_code;
}

void NanodeMQTT::disconnect()
{
   MQTT_DEBUG_PRINTLN("disconnect()");
   if (this->connected()) {
      this->state = MQTT_STATE_DISCONNECTING;
      this->tcp_transmit();
   }
}

void NanodeMQTT::publish(const char* topic, const char* payload)
{
   publish(topic, (uint8_t*)payload, strlen(payload));
}

void NanodeMQTT::publish(const char* topic, const uint8_t* payload, uint8_t plength)
{
   publish(topic, payload, plength, 0);
}

void NanodeMQTT::publish(const char* topic, const uint8_t* payload, uint8_t plength, uint8_t retained)
{
  // FIXME: check that payload isn't bigger than UIP_APPDATASIZE (or 127 bytes)
  // FIXME: can we avoid this extra buffer?
  strcpy(this->payload_topic, topic);
  memcpy(this->payload, payload, plength);
  this->payload_retain = retained;
  this->payload_length = plength;

  // If in blocking mode - loop until message has been published
  if (this->blocking_mode) {
    while (this->connected() && this->payload_length != 0) {
      uip->poll();
    }
  }
}


// ** End of the public API **


void NanodeMQTT::subscribe(const char* topic)
{
  this->subscribe_topic = topic;

  // If in blocking mode - loop until we have subscribed
  if (this->blocking_mode) {
    while (this->connected() && this->subscribe_topic != NULL) {
      uip->poll();
    }
  }
}

void NanodeMQTT::init_packet(uint8_t header)
{
  buf = (uint8_t *)uip_appdata;
  pos = 0;
  buf[pos++] = header;
  buf[pos++] = 0x00;  // Packet length
}

void NanodeMQTT::append_byte(uint8_t b)
{
  buf[pos++] = b;
}

void NanodeMQTT::append_word(uint16_t s)
{
  buf[pos++] = highByte(s);
  buf[pos++] = lowByte(s);
}

void NanodeMQTT::append_string(const char* str)
{
  // FIXME: support strings longer than 255 bytes
  const char* ptr = str;
  uint8_t len = 0;
  pos += 2;
  while (*ptr) {
    buf[pos++] = *ptr++;
    len++;
  }
  buf[pos-len-2] = 0;
  buf[pos-len-1] = len;
}

void NanodeMQTT::append_data(uint8_t *data, uint8_t data_len)
{
  memcpy(&buf[pos], data, data_len);
  pos += data_len;
}

void NanodeMQTT::send_packet()
{
  // FIXME: support packets longer than 127 bytes
  // Store the size of the packet (minus the fixed header)
  buf[1] = pos - 2;
  uip_send(buf, pos);

  // Restart the packet send timer
  timer_restart(&this->transmit_timer);
}

void NanodeMQTT::tcp_connected()
{
  this->state = MQTT_STATE_CONNECTING;
  this->tcp_transmit();
}

void NanodeMQTT::tcp_acked()
{
  switch(this->state) {
    case MQTT_STATE_CONNECTING:
      this->state = MQTT_STATE_CONNECT_SENT;
      break;
    case MQTT_STATE_PUBLISHING:
      this->state = MQTT_STATE_CONNECTED;
      this->payload_length = 0;
      break;
    case MQTT_STATE_PINGING:
      this->state = MQTT_STATE_CONNECTED;
      this->ping_pending = 0;
      break;
    case MQTT_STATE_SUBSCRIBING:
      this->state = MQTT_STATE_SUBSCRIBE_SENT;
      this->subscribe_topic = NULL;
      break;
    case MQTT_STATE_DISCONNECTING:
      this->state = MQTT_STATE_DISCONNECTED;
      uip_close();
      break;
    default:
      MQTT_DEBUG_PRINTLN("TCP: ack in unknown state");
      break;
  }
}

void NanodeMQTT::tcp_receive()
{
  uint8_t *buf = (uint8_t *)uip_appdata;
  uint8_t type = buf[0] & 0xF0;

  if (uip_datalen() == 0)
    return;

  // FIXME: check that packet isn't too long?
  // FIXME: support packets longer than 127 bytes
  // FIXME: support multiple MQTT packets in single IP packet

  switch(type) {
    case MQTT_TYPE_CONNACK: {
      uint8_t code = buf[3];
      if (code == 0) {
        MQTT_DEBUG_PRINTLN("MQTT: Connected!");
        this->state = MQTT_STATE_CONNECTED;
      } else {
        MQTT_DEBUG_PRINTF("MQTT: Connection failed (%u)\n", code);
        uip_close();
        this->state = MQTT_STATE_DISCONNECTED;
        this->error_code = code;
      }
      break;
    }
    case MQTT_TYPE_SUBACK:
      MQTT_DEBUG_PRINTLN("MQTT: Subscribed!");
      // FIXME: check current state before changing state
      this->state = MQTT_STATE_CONNECTED;
      break;
    case MQTT_TYPE_PINGRESP:
      MQTT_DEBUG_PRINTLN("MQTT: Pong!");
      this->ping_pending = 0;
      break;
    case MQTT_TYPE_PUBLISH:
      if (this->callback) {
        uint8_t tl = buf[3];
        // FIXME: is there a way we can NULL-terminate the string in the packet buffer?
        char topic[tl+1];
        memcpy(topic, &buf[4], tl);
        topic[tl] = 0;
        this->callback(topic, buf+4+tl, buf[1]-2-tl);
      }
      break;
    default:
      MQTT_DEBUG_PRINTF("MQTT: received unknown packet type (%u)\n", (type >> 4));
      break;
  }

  // Restart the packet receive timer
  // FIXME: this should only be restarted when a valid packet is received
  timer_restart(&this->receive_timer);
}

void NanodeMQTT::tcp_closed()
{
  this->state = MQTT_STATE_DISCONNECTED;
  uip_close();

  // FIXME: re-establish connection automatically
}

void NanodeMQTT::poll()
{
  if (this->state == MQTT_STATE_CONNECTED) {
    if (this->payload_length) {
      this->state = MQTT_STATE_PUBLISHING;
      this->tcp_transmit();
    } else if (this->subscribe_topic) {
      this->state = MQTT_STATE_SUBSCRIBING;
      this->message_id++;
      this->tcp_transmit();
    } else if (this->ping_pending) {
      this->state = MQTT_STATE_PINGING;
      this->tcp_transmit();
    }
  }
}

void NanodeMQTT::check_timeout()
{
  #ifdef MQTT_DEBUG
  if (timer_expired(&this->transmit_timer))
    MQTT_DEBUG_PRINTF("MQTT: not transmitted for %u seconds\n", this->keep_alive);

  if (timer_expired(&this->receive_timer))
    MQTT_DEBUG_PRINTF("MQTT: not received for %u seconds\n", this->keep_alive);
  #endif

  if (timer_expired(&this->transmit_timer) || timer_expired(&this->receive_timer)) {
    if (this->connected()) {
      if (!this->ping_pending) {
        // Send ping on the next poll
        this->ping_pending = 1;

        // Give some extra time to send and receive the ping
        // FIXME: think of a better way of doing this - takes too long to timeout
        timer_restart(&this->receive_timer);
        timer_restart(&this->transmit_timer);
      } else {
        MQTT_DEBUG_PRINTLN("MQTT: Timed out.");
        this->disconnect();
      }
    } else {
      MQTT_DEBUG_PRINTLN("MQTT: Aborting after time-out.");
      this->state = MQTT_STATE_DISCONNECTED;
      uip_abort();
    }
  }
}


// Transmit/re-transmit packet for the current state
void NanodeMQTT::tcp_transmit()
{
  switch(this->state) {
    case MQTT_STATE_CONNECTING:
      MQTT_DEBUG_PRINTLN("MQTT: sending CONNECT");
      init_packet(MQTT_TYPE_CONNECT);
      append_string("MQIsdp");
      append_byte(MQTT_PROTOCOL_VERSION);
      append_byte(MQTT_FLAG_CLEAN); // Connect flags
      append_word(this->keep_alive);
      append_string(this->client_id);
      send_packet();
      break;
    case MQTT_STATE_PUBLISHING: {
      uint8_t header = MQTT_TYPE_PUBLISH;
      if (payload_retain)
        header |= MQTT_FLAG_RETAIN;

      MQTT_DEBUG_PRINTLN("MQTT: sending PUBLISH");
      init_packet(header);
      append_string(this->payload_topic);
      append_data(this->payload, this->payload_length);
      send_packet();
      break;
    }
    case MQTT_STATE_SUBSCRIBING:
      MQTT_DEBUG_PRINTLN("MQTT: sending SUBSCRIBE");
      init_packet(MQTT_TYPE_SUBSCRIBE);
      append_word(this->message_id);
      append_string(this->subscribe_topic);
      append_byte(0x00);  // We only support QOS 0
      send_packet();
      break;
    case MQTT_STATE_PINGING:
      MQTT_DEBUG_PRINTLN("MQTT: sending PINGREQ");
      init_packet(MQTT_TYPE_PINGREQ);
      send_packet();
      break;
    case MQTT_STATE_DISCONNECTING:
      MQTT_DEBUG_PRINTLN("MQTT: sending DISCONNECT");
      init_packet(MQTT_TYPE_DISCONNECT);
      send_packet();
      break;
    default:
      MQTT_DEBUG_PRINTF("MQTT: Unable to transmit in state %u.\n", this->state);
      break;
  }
}
