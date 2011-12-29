#include <NanodeUNIO.h>
#include <NanodeUIP.h>
#include <NanodeMQTT.h>

NanodeMQTT mqtt(&uip);
struct timer my_timer;

void setup() {
  char buf[20];
  byte macaddr[6];
  uip_ipaddr_t remote_addr;
  NanodeUNIO unio(NANODE_MAC_DEVICE);

  Serial.begin(38400);
  Serial.println("MQTT Publish test");
  
  unio.read(macaddr, NANODE_MAC_ADDRESS, 6);
  uip.init(macaddr);
   
  // FIXME: use DHCP instead
  uip.set_ip_addr(10, 100, 10, 10);
  uip.set_netmask(255, 255, 255, 0);

  uip.wait_for_link();
  Serial.println("Link is up");

  // Setup a timer - publish every 5 seconds
  timer_set(&my_timer, CLOCK_SECOND * 5);

  // FIXME: resolve using DNS instead
  mqtt.set_server_addr(10, 100, 10, 1);
  mqtt.connect();


  Serial.println("setup() done");
}

void loop() {
  uip.poll();

  if(timer_expired(&my_timer)) {
    timer_reset(&my_timer);
    Serial.println("publishing.");
  }
}

