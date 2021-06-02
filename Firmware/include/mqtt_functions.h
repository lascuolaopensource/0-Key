
void onMqttPublish(uint16_t packetId) {
  SerialMon.println("Publish acknowledged.");
  SerialMon.print("  packetId: ");
  SerialMon.println(packetId);
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  SerialMon.println("Subscribe acknowledged.");
  SerialMon.print("  packetId: ");
  SerialMon.println(packetId);
  SerialMon.print("  qos: ");
  SerialMon.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  SerialMon.println("Unsubscribe acknowledged.");
  SerialMon.print("  packetId: ");
  SerialMon.println(packetId);
}