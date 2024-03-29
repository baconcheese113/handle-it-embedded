#ifndef HUB_NETWORK_REQUESTS_H
#define HUB_NETWORK_REQUESTS_H

#include "network.h"

struct sensor_details_t {
  uint8_t battery_level;
  uint16_t battery_volts;
  char firmware_version[10];
};

class NetworkRequests
{
private:
  Network* network;

public:

  /**
   * @brief Initialize this class with a ptr to network
   * @param network the ptr which will be used for direct network requests
   */
  void init(Network* network);

  /**
   * @brief Callback for when a phone is trying to register this hub
  * @param user_id the user id sent from the phone during connection
  * @param hub_addr the MAC of the hub to be used by the phone to find the hub
  * @param out_hub_id a pointer to the location to store the hub_id on success
  * @param out_result_msg a pointer to the error message returned from the network call if failed
  * @return 0 on success, -1 if failed to send
  */
  int handle_get_token_and_hub_id(char* user_id, char* hub_addr, uint16_t* out_hub_id, char* out_result_msg);
  /**
   * @brief Callback for when an event should be sent
   * @param sensor_addr MAC address of sensor responsible for event
   * @param sensor_details the details of the sensor responsible for the event
   * @return 0 on success, -1 if failed to send
   */
  int handle_send_event(char* sensor_addr, sensor_details_t* sensor_details);

  /**
   * @brief Callback for adding new sensor
   * @param sensor_addr MAC address of sensor to add
   * @param sensor_details the details of the sensor to add
   * @param door_column the column of the door the sensor is on
   * @param door_row the row of the door the sensor is on
   * @param out_result_msg a pointer to the error message returned from the network call if failed
   * @return 0 on success, -1 if failed to send
   */
  int handle_add_new_sensor(char* sensor_addr, sensor_details_t* sensor_details, uint8_t door_column, uint8_t door_row, char* out_result_msg);

  /**
   * @brief Callback for notifying the server of the current battery level
   * @param real_mV the millivolts of the battery (double the measured millivolts)
   * @param percent the estimated percentage remaining (0 - 100)
   * @return 0 on success, -1 if failed to send
   */
  int handle_update_battery_level(int real_mV, uint8_t percent);

  /**
   * @brief Callback for notifying the server of the current gps location data
   * @param lat Latitude as 45.1234
   * @param lng Longitude as 105.1234
   * @param hdop Horizontal Dilution of Precision as [0,99.9]
   * @param speed KM/hour as [0,999.99]
   * @param course Course Over Ground as [0,360.00]
   * @param real_mV the millivolts of the battery (double the measured millivolts)
   * @param percent the estimated percentage remaining (0 - 100)
   * @return 0 on success, -1 if failed to send
   */
  int handle_update_gps_loc(float lat, float lng, float hdop, float speed, float course, int real_mV, uint8_t percent);
};

#endif
