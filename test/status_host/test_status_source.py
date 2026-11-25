"""Host regression checks for the production status-packet builder."""
import subprocess
import tempfile
import unittest
from pathlib import Path


class StatusSourceTests(unittest.TestCase):
    def test_status_identity_and_transport(self):
        source = (Path(__file__).resolve().parents[2] / "src/utils.cpp").read_text()
        start = source.index("    void processStatus() {")
        end = source.index("\n    }", start) + len("\n    }")
        harness = r'''
#include <cassert>
#include <string>
struct String : std::string {
    using std::string::string;
    String(const std::string& value) : std::string(value) {}
    bool isEmpty() const { return empty(); }
};
struct {
    String callsign = "N7UV", tacticalCallsign;
    struct {
        bool sendViaAPRSIS = false, sendViaRF = true;
        String path = "WIDE2-1", statusPacket = "REBOOT COMPLETE";
    } beacon;
    struct { bool active = true; } aprs_is;
} Config;
struct Network { bool isWiFiConnected = true; bool isConnected() { return isWiFiConnected; } };
Network network;
Network* networkManager = &network;
bool statusUpdate = true;
unsigned lastStatusTx = 0, now = 123;
unsigned millis() { return now; }
std::string rf, internet;
namespace APRSPacketLib {
String generateBasePacket(const String& call, const char* destination, const String& path) {
    return String(call + ">" + destination + "," + path);
}
}
namespace APRS_IS_Utils { void upload(const String& packet) { internet = packet; } }
namespace SYSLOG_Utils { void logAPRSISTx(const String&) {} }
namespace STATION_Utils {
void addToOutputPacketBuffer(const String& packet, bool beacon) { assert(beacon); rf = packet; }
}
'''
        harness += source[start:end]
        harness += r'''
int main() {
    for (const char* tactical : {"", "TSTQRP"}) {
        Config.tacticalCallsign = tactical;
        std::string expected = *tactical ? tactical : "N7UV";
        for (bool useInternet : {false, true}) {
            Config.beacon.sendViaAPRSIS = useInternet;
            statusUpdate = true; rf.clear(); internet.clear();
            processStatus();
            assert(!statusUpdate && lastStatusTx == now);
            std::string packet = expected + ">APLRG1,WIDE2-1"
                + (useInternet ? ",qAC:>" : ":>") + "REBOOT COMPLETE";
            assert((useInternet ? internet : rf) == packet);
            assert((useInternet ? rf : internet).empty());
        }
    }
    Config.beacon.sendViaAPRSIS = false;
    Config.beacon.sendViaRF = false;
    rf.clear(); internet.clear(); statusUpdate = true;
    processStatus();
    assert(!statusUpdate && rf.empty() && internet.empty());
}
'''
        with tempfile.TemporaryDirectory() as folder:
            binary = str(Path(folder) / "status-source")
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-x", "c++", "-", "-o", binary],
                input=harness, text=True, check=True,
            )
            subprocess.run([binary], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
