NanodeMQTT
==========

Implementation of [MQTT] for the [Nanode]. MQTT is a lightweight PubSub protocol.
Nanode is an open source Arduino-like board that has in-built internet connectivity.


Requirements
------------

* [NanodeUIP] - [uIP] stack for Nanode
* [NanodeUNIO] - MAC address reading for Nanode


Limitations
-----------

- Maximum packet size is 127 bytes
- Only QOS 0 supported


TODO
----

* Implement Pinging
* Implement Publishing
* Get DNS working in example
* Assert that UIP buffers are big enough
* Implement subscribing

* Separate out UIP code, so that it works with plain [Contiki].



[MQTT]:        http://mqtt.org/
[Nanode]:      http://nanode.eu/
[NanodeUIP]:   http://github.com/sde1000/NanodeUIP
[NanodeUNIO]:  http://github.com/sde1000/NanodeUNIO
[uIP]:         http://en.wikipedia.org/wiki/UIP_(micro_IP)
[Contiki]:     http://www.contiki-os.org/
