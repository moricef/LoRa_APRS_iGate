"""Run the production serial command loop against a host-side USB stub."""
import subprocess
import tempfile
import unittest
from pathlib import Path


class SerialCommandsTests(unittest.TestCase):
    def test_closed_empty_and_readable_console(self):
        source = (Path(__file__).resolve().parents[2] / "src/rf_ota.cpp").read_text()
        start = source.index("void serialCommands() {")
        end = source.index("\n}\n}\nnamespace RF_OTA", start) + 2
        function = source[start:end]
        harness = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
struct SerialStub {
    bool closed = false;
    std::string input;
    unsigned reads = 0;
    int available() { return closed ? -1 : int(input.size()); }
    int read() {
        if (available() <= 0) throw std::runtime_error("read without data");
        ++reads;
        char c = input.front(); input.erase(0, 1); return c;
    }
} Serial;
char serialInput[32]{};
unsigned serialLength = 0;
bool serialOverflow = false;
std::vector<std::string> commands;
bool bridgeCommand(const char* line) { commands.emplace_back(line); return true; }
uint32_t now = 1;
uint32_t millis() { return now; }
unsigned kicks = 0;
void kickUsbTx() { ++kicks; }
bool bridgeOn = false;
'''
        harness += function
        harness += r'''
int main() {
    Serial.closed = true;
    serialCommands();
    assert(Serial.reads == 0 && commands.empty());
    Serial.closed = false;
    serialCommands();
    assert(Serial.reads == 0);
    Serial.input = "BRIDGE OFF\r\n";
    serialCommands();
    assert(commands.size() == 1 && commands[0] == "BRIDGE OFF");
    Serial.input = "OTA"; serialCommands();
    Serial.closed = true; serialCommands();
    Serial.closed = false; Serial.input = "BRIDGE\n"; serialCommands();
    assert(commands.size() == 2 && commands[1] == "OTABRIDGE");
    Serial.input = std::string(40, 'X') + "\nBRIDGE OFF\n";
    serialCommands();
    assert(commands.size() == 3 && commands[2] == "BRIDGE OFF");
    // USB send re-arm: after each line, while bridging, and for 200 ms after a command.
    now += 1000; unsigned k = kicks;
    serialCommands(); assert(kicks == k);
    bridgeOn = true; serialCommands(); assert(kicks == k + 1);
    bridgeOn = false; Serial.input = "PROFILE 0\n"; serialCommands(); assert(kicks == k + 3);
    now += 150; serialCommands(); assert(kicks == k + 4);
    now += 100; serialCommands(); assert(kicks == k + 4);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            binary = str(Path(folder) / "serial-commands")
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-x", "c++", "-", "-o", binary],
                input=harness, text=True, check=True,
            )
            subprocess.run([binary], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
