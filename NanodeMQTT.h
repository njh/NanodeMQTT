#ifndef NanodeMQTT_h
#define NanodeMQTT_h

#include <NanodeUIP.h>


#define MQTT_DEFAULT_PORT        (1883)
#define MQTT_DEFAULT_KEEP_ALIVE  (10)

#define MQTT_MAX_CLIENT_ID_LEN   (23)


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
  MQTT_STATE_TCP_CONNECTED,  // TCP Connected by not sent CONNECT packet yet
  MQTT_STATE_CONNECT_SENT,   // Connect packet TCP-acked
  MQTT_STATE_CONNECTED,      // Received CONNACK
  MQTT_STATE_CONNECT_FAIL,   // CONNACK returned non-zero
  MQTT_STATE_DISCONNECTED
};


class NanodeMQTT {
private:
  NanodeUIP *uip;
  char client_id[MQTT_MAX_CLIENT_ID_LEN+1];
  uip_ipaddr_t addr;
  u16_t port;
  u16_t keep_alive;
  u8_t state;

public:
  NanodeMQTT(NanodeUIP *uip);

  void set_client_id(const char* client_id);
  void set_server_addr(byte a, byte b, byte c, byte d);
  void set_server_port(u16_t port);
  
  void connect();


  void publish(char* topic, char* payload);
  void publish(char* topic, uint8_t* payload, uint8_t plength);
  void publish(char* topic, uint8_t* payload, uint8_t plength, uint8_t retained);
  

  uint8_t write_string(char* string, u8_t *buf, uint8_t *pos);

  // Callbacks
  void tcp_closed();
  void tcp_connected();
  void tcp_acked();
  void tcp_receive();
  void senddata();
  
  
  private:
  // Packet assembly functions
  void init_packet(u8_t type, u8_t flags=MQTT_FLAG_QOS_0);
  void append_byte(u8_t b);
  void append_short(u16_t s);
  void append_string(char* str);
  void send_packet();
};


struct mqtt_app_state {
  NanodeMQTT* mqtt;
};



#endif
