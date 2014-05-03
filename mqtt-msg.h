#include <stdint.h>

enum mqtt_message_type
{
  MQTT_MSG_TYPE_CONNECT     = 1,
  MQTT_MSG_TYPE_CONNACK     = 2,
  MQTT_MSG_TYPE_PUBLISH     = 3,
  MQTT_MSG_TYPE_PUBACK      = 4,
  MQTT_MSG_TYPE_PUBREC      = 5,
  MQTT_MSG_TYPE_PUBREL      = 6,
  MQTT_MSG_TYPE_PUBCOMP     = 7,
  MQTT_MSG_TYPE_SUBSCRIBE   = 8,
  MQTT_MSG_TYPE_SUBACK      = 9,
  MQTT_MSG_TYPE_UNSUBSCRIBE = 10,
  MQTT_MSG_TYPE_UNSUBACK    = 11,
  MQTT_MSG_TYPE_PINGREQ     = 12,
  MQTT_MSG_TYPE_PINGRESP    = 13,
  MQTT_MSG_TYPE_DISCONNECT  = 14
};

typedef struct mqtt_message
{
  uint8_t* data;
  uint16_t length;

} mqtt_message_t;

typedef struct mqtt_connection
{
  mqtt_message_t message;

  uint16_t message_id;
  uint8_t* buffer;
  uint16_t buffer_length;

} mqtt_connection_t;

typedef struct mqtt_connect_info
{
  char* client_id;
  char* username;
  char* password;
  char* will_topic;
  char* will_message;
  int keepalive;
  int will_qos;
  int will_retain;
  int clean_session;

} mqtt_connect_info_t;


static inline int mqtt_get_type(uint8_t* buffer)   { return (buffer[0] & 0xf0) >> 4; }
static inline int mqtt_get_dup(uint8_t* buffer)    { return (buffer[0] & 0x08) >> 3; }
static inline int mqtt_get_qos(uint8_t* buffer)    { return (buffer[0] & 0x06) >> 1; }
static inline int mqtt_get_retain(uint8_t* buffer) { return (buffer[0] & 0x01); }

void mqtt_msg_init(mqtt_connection_t* connection, uint8_t* buffer, uint16_t buffer_length);
const char* mqtt_get_publish_topic(uint8_t* buffer, uint16_t* length);
const char* mqtt_get_publish_data(uint8_t* buffer, uint16_t* length);
uint16_t mqtt_get_id(uint8_t* buffer, uint16_t length);

mqtt_message_t* mqtt_msg_connect(mqtt_connection_t* connection, mqtt_connect_info_t* info);
mqtt_message_t* mqtt_msg_publish(mqtt_connection_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id);
mqtt_message_t* mqtt_msg_puback(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrec(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrel(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubcomp(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_subscribe(mqtt_connection_t* connection, const char* topic, int qos, uint16_t* message_id);
mqtt_message_t* mqtt_msg_unsubscribe(mqtt_connection_t* connection, const char* topic, uint16_t* message_id);
mqtt_message_t* mqtt_msg_pingreq(mqtt_connection_t* connection);
mqtt_message_t* mqtt_msg_pingresp(mqtt_connection_t* connection);
mqtt_message_t* mqtt_msg_disconnect(mqtt_connection_t* connection);

