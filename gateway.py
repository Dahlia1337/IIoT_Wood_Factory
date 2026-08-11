import json
import logging
import sys
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
from colorama import Fore, Style, init
import minimalmodbus
import serial

# Initialize colorama for cross-platform colored terminal logging
init(autoreset=True)

# ==============================================================================
# LOCAL CONFIGURATION CONSTANTS
# ==============================================================================
# Southbound Configuration (Modbus RTU)
MODBUS_PORT = "COM2"
MODBUS_BAUDRATE = 115200
MODBUS_TIMEOUT = 1.0  # seconds
SLAVE_ID = 1
POLL_INTERVAL = 1.0  # seconds

# Northbound Configuration (LOCAL MQTT BROKER)
# Đổi thành IP local của Broker (Ví dụ: "127.0.0.1", "192.168.1.100", v.v.)
MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
MQTT_TOPIC = "factory/woodworking/node01/telemetry"
DEVICE_ID = "WOOD_FACTORY_NODE_01"

# Logging setup
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)


# ==============================================================================
# COLORFUL LOGGING HELPERS
# ==============================================================================
def log_info(msg: str) -> None:
    """Print formatted info log."""
    print(f"{Fore.CYAN}[INFO] {datetime.now().strftime('%H:%M:%S')} - {msg}{Style.RESET_ALL}")


def log_success(msg: str) -> None:
    """Print formatted success log."""
    print(f"{Fore.GREEN}[SUCCESS] {datetime.now().strftime('%H:%M:%S')} - {msg}{Style.RESET_ALL}")


def log_warn(msg: str) -> None:
    """Print formatted warning log."""
    print(f"{Fore.YELLOW}[WARN] {datetime.now().strftime('%H:%M:%S')} - {msg}{Style.RESET_ALL}")


def log_error(msg: str) -> None:
    """Print formatted error log."""
    print(f"{Fore.RED}[ERROR] {datetime.now().strftime('%H:%M:%S')} - {msg}{Style.RESET_ALL}")


# ==============================================================================
# LOCAL IIOT GATEWAY CLASS
# ==============================================================================
class IIoTLocalGateway:
    """
    Local Industrial Gateway class to read telemetry from Modbus RTU Slave
    and publish processed JSON data to Local MQTT Broker.
    """

    def __init__(self) -> None:
        self.modbus_instrument: minimalmodbus.Instrument | None = None
        self.mqtt_client: mqtt.Client | None = None
        self.is_mqtt_connected: bool = False

    # --------------------------------------------------------------------------
    # MQTT CONNECTION MANAGEMENT
    # --------------------------------------------------------------------------
    def _on_mqtt_connect(self, client, userdata, flags, rc, properties=None) -> None:
        """Callback triggered upon Local MQTT connection result."""
        if rc == 0:
            self.is_mqtt_connected = True
            log_success(f"Connected to Local MQTT Broker at {MQTT_BROKER}:{MQTT_PORT}")
        else:
            self.is_mqtt_connected = False
            log_error(f"Failed to connect to Local MQTT Broker. Return code: {rc}")

    def _on_mqtt_disconnect(self, client, userdata, rc, properties=None) -> None:
        """Callback triggered upon Local MQTT disconnection."""
        self.is_mqtt_connected = False
        log_warn("Disconnected from Local MQTT Broker. Attempting automatic reconnection...")

    def setup_mqtt(self) -> None:
        """Initialize Local MQTT Client and start background network loop."""
        try:
            self.mqtt_client = mqtt.Client(
                client_id=f"LocalGateway_{DEVICE_ID}",
                protocol=mqtt.MQTTv311,
            )
            self.mqtt_client.on_connect = self._on_mqtt_connect
            self.mqtt_client.on_disconnect = self._on_mqtt_disconnect

            log_info(f"Connecting to Local MQTT Broker ({MQTT_BROKER}:{MQTT_PORT})...")
            self.mqtt_client.connect_async(MQTT_BROKER, MQTT_PORT, keepalive=60)
            self.mqtt_client.loop_start()
        except Exception as e:
            log_error(f"Local MQTT Setup Error: {e}")

    # --------------------------------------------------------------------------
    # MODBUS RTU CONNECTION MANAGEMENT
    # --------------------------------------------------------------------------
    def setup_modbus(self) -> bool:
        """Initialize or re-initialize Modbus RTU instrument."""
        try:
            self.modbus_instrument = minimalmodbus.Instrument(MODBUS_PORT, SLAVE_ID)
            self.modbus_instrument.serial.baudrate = MODBUS_BAUDRATE
            self.modbus_instrument.serial.timeout = MODBUS_TIMEOUT
            self.modbus_instrument.mode = minimalmodbus.MODE_RTU
            self.modbus_instrument.clear_buffers_before_each_transaction = True
            log_success(f"Modbus RTU initialized on port {MODBUS_PORT} (Baudrate: {MODBUS_BAUDRATE})")
            return True
        except serial.SerialException as e:
            log_error(f"Modbus Serial Port Error ({MODBUS_PORT}): {e}")
            self.modbus_instrument = None
            return False
        except Exception as e:
            log_error(f"Unexpected Modbus setup error: {e}")
            self.modbus_instrument = None
            return False

    # --------------------------------------------------------------------------
    # DATA PROCESSING & PUBLISHING
    # --------------------------------------------------------------------------
    def read_modbus_telemetry(self) -> list[int] | None:
        """Read 4 Holding Registers (FC03) from Slave ID."""
        if not self.modbus_instrument:
            return None

        try:
            registers = self.modbus_instrument.read_registers(
                registeraddress=0,
                numberofregisters=4,
                functioncode=3,
            )
            return registers
        except (minimalmodbus.ModbusException, serial.SerialException) as e:
            log_error(f"Modbus Read Failed: {e}")
            return None
        except Exception as e:
            log_error(f"Unexpected error during Modbus read: {e}")
            return None

    def build_payload(self, raw_data: list[int]) -> dict:
        """Parse raw register data into structured JSON telemetry object."""
        temp_raw = raw_data[0]
        vib_raw = raw_data[1]
        dust_raw = raw_data[2]
        status_raw = raw_data[3]

        # Register 1 conversion: uint16 -> float (divided by 10.0)
        vibration_g = round(vib_raw / 10.0, 2)

        # Build ISO-8601 UTC timestamp
        current_utc_time = (
            datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        )

        payload = {
            "device_id": DEVICE_ID,
            "timestamp": current_utc_time,
            "telemetry": {
                "temperature": temp_raw,
                "vibration": vibration_g,
                "dust_density": dust_raw,
                "alarm_status": status_raw,
            },
        }
        return payload

    def publish_telemetry(self, payload: dict) -> None:
        """Publish JSON payload to Local MQTT Topic."""
        if not self.is_mqtt_connected or self.mqtt_client is None:
            log_warn("Local MQTT client not connected. Skipping payload publication.")
            return

        json_payload = json.dumps(payload)
        result = self.mqtt_client.publish(MQTT_TOPIC, json_payload, qos=1)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            log_success(f"MQTT Published Local -> [{MQTT_TOPIC}]: {json_payload}")
        else:
            log_error(f"MQTT Publish failed with error code: {result.rc}")

    # --------------------------------------------------------------------------
    # MAIN APPLICATION LOOP
    # --------------------------------------------------------------------------
    def run(self) -> None:
        """Main execution loop for Local Gateway operation."""
        log_info("Starting IIoT Local Gateway...")
        self.setup_mqtt()

        while True:
            # Handle Modbus Reconnection
            if self.modbus_instrument is None:
                log_warn("Retrying Modbus Serial Connection...")
                if not self.setup_modbus():
                    time.sleep(2.0)
                    continue

            # Execute Modbus Read Cycle
            log_info(f"Polling Modbus Slave #{SLAVE_ID} (FC03, Start Reg: 0, Count: 4)...")
            raw_registers = self.read_modbus_telemetry()

            if raw_registers is not None:
                log_info(f"Raw Registers Received: {raw_registers}")
                payload = self.build_payload(raw_registers)
                self.publish_telemetry(payload)
            else:
                log_warn("Modbus read failed or port disconnected. Resetting Modbus handle...")
                self.modbus_instrument = None

            time.sleep(POLL_INTERVAL)


# ==============================================================================
# ENTRY POINT
# ==============================================================================
if __name__ == "__main__":
    gateway = IIoTLocalGateway()
    try:
        gateway.run()
    except KeyboardInterrupt:
        log_info("Local Gateway stopped gracefully by user. Exiting...")
        if gateway.mqtt_client:
            gateway.mqtt_client.loop_stop()
            gateway.mqtt_client.disconnect()
        sys.exit(0)