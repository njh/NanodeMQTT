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
- Currently only possible to reliably subscribe to a single topic
- Only QOS 0 supported


Questions
---------

* Is the public API simple enough?
* What is the best approach to error handling?
 - Is it a good idea to print debug messages to Serial?
* What assumptions is it safe to make to reduce complexity?
 - Should we bother waiting for SUBACK?
* When/how to auto-reconnect?
* How to analyse RAM / programme usage and optimise?


TODO
----

* Fix problem with random disconnect
* Implement auto-reconnect
* Look at removing some buffers
* Get DNS working in example
* Assert that UIP buffers are big enough
* Memory usage optimisations (especially RAM)
* Implement Wills
* Improve subscribing to multiple topics
* Make a State Machine diagram
* Separate out UIP code, so that it works with plain [Contiki].
* Implement unsubscribe (?)
* Provide callback for periodic publishes?


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