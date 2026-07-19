// Host unit test for the pure hidden-camera SSID heuristic.
//   g++ -std=c++17 test/test_camera_detect.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/camera_detect.h"
#include <cassert>
#include <cstring>

int main() {
  assert(is_camera_ssid("MyIPCAM-123"));
  assert(is_camera_ssid("wyze_cam_ABC"));
  assert(is_camera_ssid("V380_1234"));       // case-insensitive
  assert(is_camera_ssid("office-CCTV"));
  assert(!is_camera_ssid("HomeWiFi"));
  assert(!is_camera_ssid(""));
  assert(!is_camera_ssid(nullptr));
  assert(strcmp(camera_ssid_brand("cam-EZVIZ-9"), "ezviz") == 0);
  return 0;
}
