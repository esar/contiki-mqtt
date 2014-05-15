#include "mqtt-service.h"
 
PROCESS_THREAD(mqtt_client_process, ev, data)
{
    static uip_ip6addr_t server_address;
 
    // Allocate buffer space for the MQTT client
    static uint8_t in_buffer[64];
    static uint8_t out_buffer[64];
 
    // Setup an mqtt_connect_info_t structure to describe
    // how the connection should behave
    static mqtt_connect_info_t connect_info =
    {
        .client_id = "contiki",
        .username = NULL,
        .password = NULL,
        .will_topic = NULL,
        .will_message = NULL,
        .keepalive = 60,
        .will_qos = 0,
        .will_retain = 0,
        .clean_session = 1
    };
 
    // The list of topics that we want to subscribe to
    static const char* topics[] =
    {
      "0", "1", "2", "3", "4", "5", NULL
    };
 
    PROCESS_BEGIN();
 
    // Set the server address
    uip_ip6addr(&server_address,
                0xbbbb, 0, 0, 0, 0, 0, 0, 0x110);
 
    // Initialise the MQTT client
    mqtt_init(in_buffer, sizeof(in_buffer),
              out_buffer, sizeof(out_buffer));
 
    // Ask the client to connect to the server
    // and wait for it to complete.
    mqtt_connect(&server_address, UIP_HTONS(1883),
                 1, &connect_info);
    PROCESS_WAIT_EVENT_UNTIL(ev == mqtt_event);
 
    if(mqtt_connected())
    {
        static int i;
 
        for(i = 0; topics[i] != NULL; ++i)
        {
            // Ask the client to subscribe to the topic
            // and wait for it to complete
            mqtt_subscribe(topics[i]);
            PROCESS_WAIT_EVENT_UNTIL(ev == mqtt_event);
        }
 
        // Loop waiting for events from the MQTT client
        while(1)
        {
            PROCESS_WAIT_EVENT_UNTIL(ev == mqtt_event);
 
            // If we've received a publish event then print
            // out the topic and message
            if(mqtt_event_is_publish(data))
            {
                const char* topic = mqtt_svc_event_get_topic(data);
                const char* message = mqtt_svc_event_get_message(data);
                int level = 0;
 
                printf("%s = %s\n", topic, message);
 
                // Relay the received message to a new topic
                mqtt_publish("new_topic", message, 0, 1);
            }
        }
    }
    else
        printf("mqtt service connect failed\n");
 
    PROCESS_END();
}
