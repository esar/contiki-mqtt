#include "mqtt-msg.h"

#define MQTT_FLAG_CONNECTED          1
#define MQTT_FLAG_READY              2
#define MQTT_FLAG_EXIT               4

#define MQTT_EVENT_TYPE_NONE         0
#define MQTT_EVENT_TYPE_CONNECTED    1
#define MQTT_EVENT_TYPE_DISCONNECTED 2
#define MQTT_EVENT_TYPE_SUBSCRIBED   3
#define MQTT_EVENT_TYPE_UNSUBSCRIBED 4
#define MQTT_EVENT_TYPE_PUBLISH      5
#define MQTT_EVENT_TYPE_PUBLISHED    6
#define MQTT_EVENT_TYPE_EXITED       7

typedef struct mqtt_event_data_t
{
  uint8_t type;
  const char* topic;
  const char* data;
  uint16_t topic_length;
  uint16_t data_length;

} mqtt_event_data_t;

extern int mqtt_flags;
extern process_event_t mqtt_event;


// Must be called before any other function is  called.
//
// Initialises the MQTT client library and associates the
// provided buffers with it. The buffer memory must remain
// valid throughout the use of the API.
void mqtt_init(uint8_t* in_buffer, int in_buffer_length, 
               uint8_t* out_buffer, int out_buffer_length);

// Starts an asynchronous connect to the server specified by
// the address and port. If auto_reconnect is non-zero then
// the it will keep trying to connect indefinitely and if the
// connection drops it will attempt to reconnect.
// The info structure provides other connection details
// such as username/password, will topic/message, etc.
// The memory pointed to by info must remain valid
// throughout the use of the API.
//
// The calling process will receive an mqtt_event of type
// MQTT_EVENT_CONNECTED when the operation is complete.
// Or an event of type MQTT_EVENT_DISCONNECTED if the
// connect attempt fails.
int mqtt_connect(uip_ip6addr_t* address, uint16_t port, 
                 int auto_reconnect, mqtt_connect_info_t* info);

// Starts an asynchronous disconnect from the server.
// The calling process will receive a mqtt_event of type
// MQTT_EVENT_TYPE_EXITED when the operation is complete.
int mqtt_disconnect();

// Starts an asynchronous subscribe to the specified topic
// The calling process will receive a mqtt_event of type
// MQTT_EVENT_TYPE_SUBSCRIBE when the servers reply has
// been received.
int mqtt_subscribe(const char* topic);

// Starts an asynchronous unsubscribe of the specified topic.
// The calling process will receive a mqtt_event of type
// MQTT_EVENT_TYPE_UNSUBSCRIBED when the server's reply
// has been received.
int mqtt_unsubscribe(const char* topic);

// Same as mqtt_publish() but the data doesn't have to be
// NULL terminated as a length is supplied instead.
int mqtt_publish_with_length(const char* topic, const char* data, int data_length, int qos, int retain);

// Starts an asynchronous publish of the specified data to
// the specified topic.
// The calling process will receive a mqtt_event of type
// MQTT_EVENT_TYPE_PUBLISHED when the operation is complete
static inline int mqtt_publish(const char* topic, const char* data, int qos, int retain)
{
  return mqtt_publish_with_length(topic, data, data != NULL ? strlen(data) : 0, qos, retain);
}


static inline int mqtt_connected()
{
  return (mqtt_flags & MQTT_FLAG_CONNECTED);
}
static inline int mqtt_ready()
{
  return (mqtt_flags & MQTT_FLAG_READY);
}

static inline int mqtt_event_is_connected(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_CONNECTED;
}
static inline int mqtt_event_is_disconnected(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_DISCONNECTED;
}
static inline int mqtt_event_is_subscribed(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_SUBSCRIBED;
}
static inline int mqtt_event_is_unsubscribed(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_UNSUBSCRIBED;
}
static inline int mqtt_event_is_publish(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_PUBLISH;
}
static inline int mqtt_event_is_published(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_PUBLISHED;
}
static inline int mqtt_event_is_exited(void* data)
{
  return ((mqtt_event_data_t*)data)->type == MQTT_EVENT_TYPE_EXITED;
}

static inline const char* mqtt_event_get_topic(void* data)
{
  return ((mqtt_event_data_t*)data)->topic; 
}
static inline uint16_t mqtt_event_get_topic_length(void* data)
{
  return ((mqtt_event_data_t*)data)->topic_length;
}
static inline const char* mqtt_event_get_data(void* data)
{
  return ((mqtt_event_data_t*)data)->data; 
}
static inline uint16_t mqtt_event_get_data_length(void* data)
{
  return ((mqtt_event_data_t*)data)->data_length;
}


