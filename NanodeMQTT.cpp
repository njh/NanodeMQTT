
#include <NanodeMQTT.h>


NanodeMQTT::NanodeMQTT(NanodeUIP *uip)
{
  this->uip = uip;
  memset(this->client_id, 0, MQTT_MAX_CLIENT_ID_LEN+1);
  this->port = MQTT_DEFAULT_PORT;
  this->keep_alive = MQTT_DEFAULT_KEEP_ALIVE;
  this->state = MQTT_STATE_DISCONNECTED;
}

static void mqtt_appcall(void)
{
  struct mqtt_app_state *s = (struct mqtt_app_state *)&(uip_conn->appstate);
  NanodeMQTT *mqtt = s->mqtt;

  if (uip_aborted() || uip_timedout() || uip_closed()) {
    mqtt->tcp_closed();
  } else if (uip_connected()) {
    mqtt->tcp_connected();
  } else if (uip_acked()) {
    mqtt->tcp_acked();
  } else if (uip_newdata()) {
    mqtt->tcp_receive();
  } else if (uip_rexmit()) {
    Serial.println("*Retransmitting*");
    mqtt->tcp_transmit();
  } else if (uip_poll()) {
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
  uip_ipaddr(this->addr, a,b,c,d);
}

void NanodeMQTT::set_server_port(u16_t port)
{
  this->port = port;
}

void NanodeMQTT::set_keep_alive(u16_t secs)
{
  this->keep_alive = secs;
}


// FIXME: add support for WILL
void NanodeMQTT::connect()
{
  struct uip_conn *conn;

  // Set the client ID to the MAC address, if none is set
  if (this->client_id[0] == '\0') {
    uip->get_mac_str(this->client_id);
  }

  conn = uip_connect(&this->addr, HTONS(this->port), mqtt_appcall);
  if (conn) {
    struct mqtt_app_state *s = (struct mqtt_app_state *)&(conn->appstate);
    s->mqtt = this;
    this->state = MQTT_STATE_WAITING;
  }

  // Set the keep-alive timers
  timer_set(&this->transmit_timer, CLOCK_SECOND * this->keep_alive);
  timer_set(&this->receive_timer, CLOCK_SECOND * this->keep_alive);
}


u8_t NanodeMQTT::connected()
{
  switch(this->state) {
    case MQTT_STATE_CONNECTED:
    case MQTT_STATE_PINGING:
      return 1;
    default:
      return 0;
  }
}

void NanodeMQTT::disconnect()
{
   Serial.println("disconnect()");
   if (this->connected()) {
      this->state = MQTT_STATE_DISCONNECTING;
      this->tcp_transmit();
   }
}

void NanodeMQTT::publish(const char* topic, char* payload)
{
   publish(topic, (uint8_t*)payload, strlen(payload));
}

void NanodeMQTT::publish(const char* topic, uint8_t* payload, uint8_t plength)
{
   publish(topic, payload, plength, 0);
}

void NanodeMQTT::publish(const char* topic, uint8_t* payload, uint8_t plength, uint8_t retained)
{
  // FIXME: check that payload isn't bigger than UIP_APPDATASIZE (or 127 bytes)
  // FIXME: can we avoid this extra buffer?
  strcpy(this->payload_topic, topic);
  memcpy(this->payload, payload, plength);
  this->payload_retain = retained;
  this->payload_length = plength;
}


void NanodeMQTT::init_packet(u8_t header)
{
  buf = (u8_t *)uip_appdata;
  pos = 0;
  buf[pos++] = header;
  buf[pos++] = 0x00;  // Packet length
}

void NanodeMQTT::append_byte(u8_t b)
{
  buf[pos++] = b;
}

void NanodeMQTT::append_word(u16_t s)
{
  // FIXME: endian is confusing the hell out of me
  buf[pos++] = (s >> 8) & 0xFF;
  buf[pos++] = s & 0xFF;
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

void NanodeMQTT::append_data(u8_t *data, u8_t data_len)
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
  this->state = MQTT_STATE_TCP_CONNECTED;
  this->tcp_transmit();
}

void NanodeMQTT::tcp_acked()
{
  if (state == MQTT_STATE_TCP_CONNECTED) {
    this->state = MQTT_STATE_CONNECT_SENT;
  } else if (state == MQTT_STATE_PUBLISHING) {
    this->state = MQTT_STATE_CONNECTED;
    this->payload_length = 0;
  } else if (state == MQTT_STATE_PINGING) {
    this->state = MQTT_STATE_CONNECTED;
  } else if (state == MQTT_STATE_DISCONNECTING) {
    this->state = MQTT_STATE_DISCONNECTED;
    uip_close();
  } else {
    Serial.println("ack unknown state");
  }
}

void NanodeMQTT::tcp_receive()
{
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t type = buf[0] & 0xF0;

  if (uip_datalen() == 0)
    return;

  // FIXME: support packets longer than 127 bytes

  if (type == MQTT_TYPE_CONNACK) {
     u8_t code = buf[3];
     if (code == 0) {
        Serial.println("Connected!");
        this->state = MQTT_STATE_CONNECTED;
     } else {
        Serial.print("Connection to MQTT failed (0x");
        Serial.print(code, HEX);
        Serial.println(")");
        uip_close();
        this->state = MQTT_STATE_CONNECT_FAIL;
     }
  } else if (type == MQTT_TYPE_PINGRESP) {
    Serial.println("Pong!");
  } else {
    // Ignore
  }

  // Restart the packet receive timer
  // FIXME: this should only be restarted when a valid packet is received
  timer_restart(&this->receive_timer);
}

void NanodeMQTT::tcp_closed()
{
  Serial.println("tcp_closed()");
  this->state = MQTT_STATE_DISCONNECTED;
  uip_close();

  // FIXME: re-establish connection automatically
}

void NanodeMQTT::poll()
{
  if (this->payload_length && this->state == MQTT_STATE_CONNECTED) {
    this->state = MQTT_STATE_PUBLISHING;
    this->tcp_transmit();
  }
}

void NanodeMQTT::check_timeout()
{
  if (timer_expired(&this->transmit_timer) || timer_expired(&this->receive_timer)) {
    if (this->state == MQTT_STATE_CONNECTED) {
      // Send a ping
      this->state = MQTT_STATE_PINGING;
      this->tcp_transmit();

      // Give the server time to respond to the ping
      timer_restart(&this->receive_timer);
    } else if (this->connected()) {
      Serial.println("Timed out.");
      this->disconnect();
    } else {
      this->state = MQTT_STATE_DISCONNECTED;
      uip_abort();
    }
  }
}


// Transmit/re-transmit packet for the current state
void NanodeMQTT::tcp_transmit()
{
  Serial.println("tcp_transmit()");

  if (this->state == MQTT_STATE_TCP_CONNECTED) {
    // Send a CONNECT packet
    init_packet(MQTT_TYPE_CONNECT);
    append_string("MQIsdp");
    append_byte(MQTT_PROTOCOL_VERSION);
    append_byte(MQTT_FLAG_CLEAN); // Connect flags
    append_word(this->keep_alive);
    append_string(this->client_id);
    send_packet();
  } else if (this->state == MQTT_STATE_PUBLISHING) {
    u8_t header = MQTT_TYPE_PUBLISH;
    if (payload_retain)
      header |= MQTT_FLAG_RETAIN;
    init_packet(header);
    append_string(this->payload_topic);
    append_data(this->payload, this->payload_length);
    send_packet();
  } else if (this->state == MQTT_STATE_PINGING) {
    init_packet(MQTT_TYPE_PINGREQ);
    send_packet();
  } else if (this->state == MQTT_STATE_DISCONNECTING) {
    init_packet(MQTT_TYPE_DISCONNECT);
    send_packet();
  } else {
    Serial.print("** unable to transmit ");
    Serial.print(this->state, DEC);
    Serial.println();
  }
}
