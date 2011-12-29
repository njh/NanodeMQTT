
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
  }

  if (uip_connected()) {
    mqtt->tcp_connected();
  }

  if (uip_acked()) {
    mqtt->tcp_acked();
  }
  
  // Is new incoming data available?
  if (uip_newdata()) {
    mqtt->tcp_receive();
  }

  if (uip_rexmit()) {
    Serial.println("uip_rexmit()");
  }
  
  if (uip_poll()) {
    Serial.println("uip_poll()");
  }

  if (uip_connected() || uip_rexmit()) {
    mqtt->senddata();
  }

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


// FIXME: add support for WILL
void NanodeMQTT::connect()
{
  struct uip_conn *conn;
  
  if (this->client_id[0] == '\0') {
    uip->get_mac_str(this->client_id);
  }

  conn = uip_connect(&this->addr, HTONS(this->port), mqtt_appcall);
  if (conn) {
    struct mqtt_app_state *s = (struct mqtt_app_state *)&(conn->appstate);
    s->mqtt = this;
    this->state = MQTT_STATE_WAITING;
  }
  

}


void NanodeMQTT::publish(char* topic, char* payload)
{
   publish(topic, (uint8_t*)payload, strlen(payload));
}

void NanodeMQTT::publish(char* topic, uint8_t* payload, uint8_t plength)
{
   publish(topic, payload, plength, 0);
}

void NanodeMQTT::publish(char* topic, uint8_t* payload, uint8_t plength, uint8_t retained)
{
    // FIXME: check that payload isn't bigger than UIP_APPDATASIZE / 127
}


void NanodeMQTT::init_packet(u8_t type, u8_t flags)
{
  u8_t *buf = (u8_t *)uip_appdata;
  buf[0] = type | flags;
  buf[1] = 0x00;  // Packet length
}

void NanodeMQTT::append_byte(u8_t b)
{
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t pos = buf[1] + 2;
  buf[pos++] = b;
  buf[1] = pos - 2;
}

void NanodeMQTT::append_short(u16_t s)
{
  // FIXME: endian is confusing the hell out of me
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t pos = buf[1] + 2;
  buf[pos++] = (s >> 8) & 0xFF;
  buf[pos++] = s & 0xFF;
  buf[1] = pos - 2;
}

void NanodeMQTT::append_string(char* str)
{
  // FIXME: support strings longer than 255 bytes
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t pos = buf[1] + 2;
  char* ptr = str;
  uint8_t len = 0;
  pos += 2;
  while (*ptr) {
    buf[pos++] = *ptr++;
    len++;
  }
  buf[pos-len-2] = 0;
  buf[pos-len-1] = len;
  buf[1] = pos - 2;
}

void NanodeMQTT::send_packet()
{
  // FIXME: support packets longer than 127 bytes
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t len = buf[1] + 2;
  uip_send(buf, len);
}

void NanodeMQTT::tcp_connected()
{
  Serial.println("tcp_connected()");
  this->state = MQTT_STATE_TCP_CONNECTED;
}

void NanodeMQTT::tcp_acked()
{
  Serial.println("tcp_acked()");
  if (state == MQTT_STATE_TCP_CONNECTED) {
    this->state = MQTT_STATE_CONNECT_SENT;
  }
}

void NanodeMQTT::tcp_receive()
{
  u8_t *buf = (u8_t *)uip_appdata;
  u8_t type = buf[0] & 0xF0;

  if (uip_datalen() == 0)
    return;

  Serial.println("tcp_receive()");
  for(u8_t i=0; i<uip_len; i++) {
    Serial.print(" 0x");
    Serial.print(i, HEX);
    Serial.print(": 0x");
    Serial.print(buf[i], HEX);
    Serial.println();
  }
  
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
  } else {
    Serial.print("unknown type=0x");
    Serial.print(type, HEX);
    Serial.println();
  }
}

void NanodeMQTT::tcp_closed()
{
  Serial.println("tcp_closed()");
  this->state = MQTT_STATE_DISCONNECTED;
  uip_close();
  
  // FIXME: re-establish connection automatically
}

void NanodeMQTT::senddata()
{
  Serial.println("senddata()");

  if (this->state == MQTT_STATE_TCP_CONNECTED) {
    // Send a CONNECT packet
    init_packet(MQTT_TYPE_CONNECT);
    append_string("MQIsdp");
    append_byte(MQTT_PROTOCOL_VERSION);
    append_byte(MQTT_FLAG_CLEAN); // Connect flags
    append_short(this->keep_alive);
    append_string(this->client_id);
    send_packet();
  }
}