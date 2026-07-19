// Host unit test for the pure SSDP/DIAL response parse.
//   g++ -std=c++17 test/test_ssdp.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ssdp.h"
#include <cassert>
#include <cstring>

int main() {
  const char *resp =
    "HTTP/1.1 200 OK\r\n"
    "CACHE-CONTROL: max-age=1800\r\n"
    "LOCATION: http://192.168.1.5:8008/ssdp/device-desc.xml\r\n"
    "ST: urn:dial-multiscreen-org:service:dial:1\r\n"
    "USN: uuid:abc::urn:dial-multiscreen-org:service:dial:1\r\n\r\n";

  char v[128];
  assert(ssdp_header(resp, "LOCATION", v, sizeof v));
  assert(strcmp(v, "http://192.168.1.5:8008/ssdp/device-desc.xml") == 0);
  // Case-insensitive key match.
  assert(ssdp_header(resp, "st", v, sizeof v));
  assert(strcmp(v, "urn:dial-multiscreen-org:service:dial:1") == 0);
  // Missing header.
  assert(!ssdp_header(resp, "SERVER", v, sizeof v));

  // Device classification.
  assert(strcmp(cast_kind("urn:dial-multiscreen-org:service:dial:1"),
                "Chromecast/DIAL") == 0);
  assert(strcmp(cast_kind("roku:ecp"), "Roku") == 0);
  assert(strcmp(cast_kind("urn:schemas-upnp-org:device:MediaRenderer:1"),
                "DLNA/UPnP") == 0);
  assert(strcmp(cast_kind("something-else"), "unknown cast device") == 0);
  return 0;
}
