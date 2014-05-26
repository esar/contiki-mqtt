MQTT client library for Contiki
===============================

This is an MQTT client library for the Contiki operating system.

It operates asynchronously, creating a new process to handle communication
with the message broker. It supports subscribing, publishing, authentication,
will messages, keep alive pings and all 3 QoS levels. In short, it should be
a fully functional client, though some areas haven't been well tested yet.

To use this library, create a mqtt-service directory in the Contiki apps
directory and place these library files within it. Then add mqtt-service to
the APPS variable in your application's makefile.

See mqtt-service.h for documentation and example.c for example usage.

For more exmples of usage, look in the devices/* directories in
http://github.com/esar/myha
