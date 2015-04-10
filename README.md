Note that I am no-longer working on this codebase.
==================================================


NanodeMQTT
----------

Implementation of [MQTT] for the [Nanode]. MQTT is a lightweight PubSub protocol.
Nanode is an open source Arduino-like board that has in-built internet connectivity.
It is licensed under the [BSD 3-Clause License].


Requirements
------------

* [NanodeUIP] - [uIP] stack for Nanode
* [NanodeUNIO] - MAC address reading for Nanode


Limitations
-----------

- Maximum packet size is 127 bytes
- Only QOS 0 supported


Thanks to
---------

* Nicholas O'Leary (Author of [PubSubClient] for Arduino)
* Stephen Early (Author of [NanodeUIP])




[MQTT]:         http://mqtt.org/
[Nanode]:       http://nanode.eu/
[NanodeUIP]:    http://github.com/sde1000/NanodeUIP
[NanodeUNIO]:   http://github.com/sde1000/NanodeUNIO
[uIP]:          http://en.wikipedia.org/wiki/UIP_(micro_IP)
[Contiki]:      http://www.contiki-os.org/
[PubSubClient]: http://knolleary.net/arduino-client-for-mqtt/
[BSD 3-Clause License]: http://www.opensource.org/licenses/BSD-3-Clause
