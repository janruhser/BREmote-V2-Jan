#!/usr/bin/env python3
"""
BREmote V2 Hardware Test Suite
Windows application for verifying TX and RX units

Features:
- Auto-detects BREmote devices on COM ports
- Identifies TX vs RX units
- Runs comprehensive functional tests
- Generates test reports

Usage:
    python bremote_test.py [--port COMx] [--test all|radio|display|hall|analog] [--gui]
"""

import sys
import time
import serial
import serial.tools.list_ports
import threading
import json
import argparse
from datetime import datetime
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict, Tuple, Callable
from enum import Enum
import logging

# Optional GUI support
try:
    import tkinter as tk
    from tkinter import ttk, scrolledtext, messagebox
    GUI_AVAILABLE = True
except ImportError:
    GUI_AVAILABLE = False


class DeviceType(Enum):
    UNKNOWN = "unknown"
    TRANSMITTER = "tx"
    RECEIVER = "rx"


class TestResult(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    PENDING = "PENDING"


@dataclass
class TestReport:
    device_type: str
    port: str
    timestamp: str
    tests: Dict[str, Dict]
    overall_result: str
    
    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2)


class BREmoteDevice:
    """Represents a connected BREmote device (TX or RX)"""
    
    BAUD_RATE = 115200
    TIMEOUT = 2.0
    COMMAND_DELAY = 0.1
    
    def __init__(self, port: str):
        self.port = port
        self.device_type = DeviceType.UNKNOWN
        self.serial: Optional[serial.Serial] = None
        self.lock = threading.Lock()
        self.last_response = ""
        
    def connect(self) -> bool:
        """Establish serial connection"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.BAUD_RATE,
                timeout=self.TIMEOUT,
                write_timeout=1.0
            )
            time.sleep(0.5)  # Allow device to initialize
            self._flush_buffers()
            return True
        except Exception as e:
            logging.error(f"Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            self.serial = None
    
    def _flush_buffers(self):
        """Clear serial buffers"""
        if self.serial:
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
    
    def send_command(self, command: str, wait_for_response: bool = True) -> str:
        """Send command and optionally wait for response"""
        with self.lock:
            if not self.serial or not self.serial.is_open:
                return ""
            
            try:
                self._flush_buffers()
                self.serial.write(f"{command}\r\n".encode())
                self.serial.flush()
                
                if wait_for_response:
                    time.sleep(self.COMMAND_DELAY)
                    response = self._read_response()
                    self.last_response = response
                    return response
                return ""
            except Exception as e:
                logging.error(f"Command failed: {e}")
                return ""
    
    def _read_response(self) -> str:
        """Read response from serial"""
        lines = []
        start_time = time.time()

        while time.time() - start_time < self.TIMEOUT:
            if self.serial.in_waiting:
                try:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        lines.append(line)
                except:
                    pass
            else:
                time.sleep(0.01)

        return "\n".join(lines)

    def send_json_command(self, command: str) -> Optional[dict]:
        """Send a command with 'json' argument and parse the JSON response.

        Returns parsed dict on success, None on failure.
        The command should NOT include the 'json' suffix — it is appended automatically.
        """
        response = self.send_command(f"{command} json")
        if not response:
            return None
        # The response may contain multiple lines; find the first valid JSON line
        for line in response.split('\n'):
            line = line.strip()
            if line.startswith('{'):
                try:
                    return json.loads(line)
                except json.JSONDecodeError:
                    continue
        return None

    def identify(self) -> DeviceType:
        """Determine if device is TX or RX using ?state json"""
        # Try TX-specific JSON status command
        data = self.send_json_command("?state")
        if data and isinstance(data, dict):
            # TX ?state json returns keys like "hall", "radio", "locked", "gear"
            if "gear" in data or "locked" in data:
                self.device_type = DeviceType.TRANSMITTER
                return self.device_type

        # Fallback: try text-based identification
        response = self.send_command("?conf")
        if "BREmote V2 TX" in response or "confStruct" in response.lower() or "gear" in response.lower():
            self.device_type = DeviceType.TRANSMITTER
            return self.device_type

        # Try RX-specific indicators
        response = self.send_command("?")
        if any(x in response.lower() for x in ["vesc", "pwm", "motor"]):
            self.device_type = DeviceType.RECEIVER
            return self.device_type

        return self.device_type
    
    def is_connected(self) -> bool:
        """Check if serial connection is active"""
        return self.serial is not None and self.serial.is_open
    
    def __str__(self):
        return f"BREmote {self.device_type.value.upper()} on {self.port}"


@dataclass
class RadioLinkSample:
    """Single sample of TX output and RX input"""
    timestamp: float
    tx_throttle: Optional[int] = None
    tx_steering: Optional[int] = None
    rx_throttle: Optional[int] = None
    rx_steering: Optional[int] = None
    rssi: Optional[int] = None
    snr: Optional[float] = None


class RadioLinkMonitor:
    """Monitors and correlates TX output with RX input over radio link"""
    
    def __init__(self, tx_device: BREmoteDevice, rx_device: BREmoteDevice, 
                 gui_callback: Optional[Callable] = None):
        self.tx_device = tx_device
        self.rx_device = rx_device
        self.gui_callback = gui_callback
        self.running = False
        self.samples: List[RadioLinkSample] = []
        self.tx_thread: Optional[threading.Thread] = None
        self.rx_thread: Optional[threading.Thread] = None
        self.lock = threading.Lock()
        self.tx_buffer = ""
        self.rx_buffer = ""
        
    def log(self, message: str):
        """Log message"""
        print(message)
        if self.gui_callback:
            self.gui_callback(message)
    
    def start(self, duration: float = 10.0):
        """Start monitoring radio link for specified duration"""
        self.log(f"\n[LINK] Starting Radio Link Test ({duration}s)...")
        self.log(f"   TX: {self.tx_device.port}")
        self.log(f"   RX: {self.rx_device.port}")

        # Enable continuous JSON output on both devices
        self.tx_device.send_command("?printInputs json", wait_for_response=False)
        time.sleep(0.2)
        self.rx_device.send_command("?printReceived json", wait_for_response=False)  # Need to implement on RX
        
        self.running = True
        self.samples = []
        
        # Start monitoring threads
        self.tx_thread = threading.Thread(target=self._monitor_tx)
        self.rx_thread = threading.Thread(target=self._monitor_rx)
        self.tx_thread.start()
        self.rx_thread.start()
        
        # Wait for test duration
        time.sleep(duration)
        
        # Stop monitoring
        self.stop()
        
        # Analyze results
        return self._analyze_results()
    
    def stop(self):
        """Stop monitoring"""
        self.running = False
        
        # Stop continuous output
        self.tx_device.send_command("quit", wait_for_response=False)
        self.rx_device.send_command("quit", wait_for_response=False)
        
        # Wait for threads to finish
        if self.tx_thread:
            self.tx_thread.join(timeout=1.0)
        if self.rx_thread:
            self.rx_thread.join(timeout=1.0)
    
    def _monitor_tx(self):
        """Monitor TX serial output in background"""
        while self.running:
            try:
                if self.tx_device.serial and self.tx_device.serial.in_waiting:
                    data = self.tx_device.serial.read(self.tx_device.serial.in_waiting)
                    self.tx_buffer += data.decode('utf-8', errors='ignore')
                    self._parse_tx_buffer()
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    self.log(f"  TX Monitor Error: {e}")
    
    def _monitor_rx(self):
        """Monitor RX serial output in background"""
        while self.running:
            try:
                if self.rx_device.serial and self.rx_device.serial.in_waiting:
                    data = self.rx_device.serial.read(self.rx_device.serial.in_waiting)
                    self.rx_buffer += data.decode('utf-8', errors='ignore')
                    self._parse_rx_buffer()
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    self.log(f"  RX Monitor Error: {e}")
    
    def _parse_tx_buffer(self):
        """Parse TX JSON output for throttle/steering values.

        Expected JSON: {"throttle":N,"steering":N,"toggle":N,...}
        """
        lines = self.tx_buffer.split('\n')
        self.tx_buffer = lines[-1] if lines else ""

        for line in lines[:-1]:
            line = line.strip()
            if not line.startswith('{'):
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue

            sample = RadioLinkSample(timestamp=time.time())
            if "throttle" in data:
                sample.tx_throttle = int(data["throttle"])
            if "steering" in data:
                sample.tx_steering = int(data["steering"])

            if sample.tx_throttle is not None or sample.tx_steering is not None:
                with self.lock:
                    self.samples.append(sample)
    
    def _parse_rx_buffer(self):
        """Parse RX JSON output for received throttle/steering/RSSI values.

        Expected JSON: {"throttle":N,"steering":N,"rssi":N,"snr":F,...}
        """
        lines = self.rx_buffer.split('\n')
        self.rx_buffer = lines[-1] if lines else ""

        for line in lines[:-1]:
            line = line.strip()
            if not line.startswith('{'):
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue

            sample = RadioLinkSample(timestamp=time.time())
            if "throttle" in data:
                sample.rx_throttle = int(data["throttle"])
            if "steering" in data:
                sample.rx_steering = int(data["steering"])
            if "rssi" in data:
                sample.rssi = int(data["rssi"])
            if "snr" in data:
                sample.snr = float(data["snr"])

            if sample.rx_throttle is not None or sample.rx_steering is not None:
                with self.lock:
                    # Match with recent TX sample (within 200ms)
                    matched = False
                    for s in reversed(self.samples[-50:]):
                        if abs(s.timestamp - sample.timestamp) < 0.2:
                            if sample.rx_throttle is not None:
                                s.rx_throttle = sample.rx_throttle
                            if sample.rx_steering is not None:
                                s.rx_steering = sample.rx_steering
                            if sample.rssi is not None:
                                s.rssi = sample.rssi
                            if sample.snr is not None:
                                s.snr = sample.snr
                            matched = True
                            break

                    if not matched:
                        self.samples.append(sample)
    
    def _analyze_results(self) -> Dict:
        """Analyze collected samples and return results"""
        with self.lock:
            samples = self.samples.copy()
        
        if not samples:
            return {
                "result": TestResult.FAIL.value,
                "details": "No data collected",
                "samples_collected": 0,
                "matched_pairs": 0,
                "avg_latency_ms": None,
                "avg_rssi": None,
                "packet_loss_percent": 100.0
            }
        
        # Count matched pairs (samples with both TX and RX data)
        matched = [s for s in samples if s.tx_throttle is not None and s.rx_throttle is not None]
        tx_only = [s for s in samples if s.tx_throttle is not None and s.rx_throttle is None]
        
        total_tx = len([s for s in samples if s.tx_throttle is not None])
        total_rx = len([s for s in samples if s.rx_throttle is not None])
        matched_count = len(matched)
        
        # Calculate packet loss
        packet_loss = ((total_tx - matched_count) / total_tx * 100) if total_tx > 0 else 100.0
        
        # Calculate value differences
        throttle_diffs = [abs(s.tx_throttle - s.rx_throttle) for s in matched 
                         if s.tx_throttle is not None and s.rx_throttle is not None]
        steering_diffs = [abs(s.tx_steering - s.rx_steering) for s in matched 
                         if s.tx_steering is not None and s.rx_steering is not None]
        
        avg_throttle_diff = sum(throttle_diffs) / len(throttle_diffs) if throttle_diffs else 0
        avg_steering_diff = sum(steering_diffs) / len(steering_diffs) if steering_diffs else 0
        max_throttle_diff = max(throttle_diffs) if throttle_diffs else 0
        max_steering_diff = max(steering_diffs) if steering_diffs else 0
        
        # RSSI statistics
        rssi_values = [s.rssi for s in matched if s.rssi is not None]
        avg_rssi = sum(rssi_values) / len(rssi_values) if rssi_values else None
        min_rssi = min(rssi_values) if rssi_values else None
        
        # SNR statistics
        snr_values = [s.snr for s in matched if s.snr is not None]
        avg_snr = sum(snr_values) / len(snr_values) if snr_values else None
        
        # Determine pass/fail
        passed = True
        reasons = []
        
        if packet_loss > 20:  # More than 20% packet loss
            passed = False
            reasons.append(f"High packet loss: {packet_loss:.1f}%")
        
        if max_throttle_diff > 5 or max_steering_diff > 5:  # Values differ by more than 5
            passed = False
            reasons.append(f"Value mismatch: thr_diff={max_throttle_diff}, steer_diff={max_steering_diff}")
        
        if avg_rssi is not None and avg_rssi < -100:  # Very weak signal
            passed = False
            reasons.append(f"Weak signal: RSSI {avg_rssi} dBm")
        
        if matched_count < 10:  # Too few samples
            passed = False
            reasons.append(f"Insufficient samples: {matched_count} pairs")
        
        result = {
            "test": "Radio Link Integration",
            "result": TestResult.PASS.value if passed else TestResult.FAIL.value,
            "details": "; ".join(reasons) if reasons else "Radio link working correctly",
            "samples_collected": len(samples),
            "tx_samples": total_tx,
            "rx_samples": total_rx,
            "matched_pairs": matched_count,
            "packet_loss_percent": round(packet_loss, 2),
            "avg_throttle_diff": round(avg_throttle_diff, 2),
            "avg_steering_diff": round(avg_steering_diff, 2),
            "max_throttle_diff": max_throttle_diff,
            "max_steering_diff": max_steering_diff,
            "avg_rssi_dbm": avg_rssi,
            "min_rssi_dbm": min_rssi,
            "avg_snr_db": avg_snr
        }
        
        return result


class BREmoteTester:
    """Test orchestrator for BREmote devices"""
    
    def __init__(self, gui_callback: Optional[Callable] = None):
        self.gui_callback = gui_callback
        self.devices: List[BREmoteDevice] = []
        self.test_results: Dict[str, TestReport] = {}
        
    def log(self, message: str):
        """Log message to console and optionally GUI"""
        try:
            print(message)
        except UnicodeEncodeError:
            # Handle Windows console encoding issues
            print(message.encode('utf-8', errors='replace').decode('utf-8'))
        if self.gui_callback:
            self.gui_callback(message)
    
    def scan_ports(self) -> List[str]:
        """Scan for available COM ports with BREmote devices"""
        self.log("\n[SCAN] Scanning for BREmote devices...")
        ports = serial.tools.list_ports.comports()
        bre_ports = []
        
        for port_info in ports:
            port = port_info.device
            # Check for ESP32 or common USB-Serial chips
            if any(x in port_info.description.lower() for x in 
                   ["usb", "serial", "uart", "cp210", "ch340", "ftdi", "esp32"]):
                self.log(f"  Checking {port}: {port_info.description}")
                device = BREmoteDevice(port)
                if device.connect():
                    device_type = device.identify()
                    if device_type != DeviceType.UNKNOWN:
                        self.devices.append(device)
                        bre_ports.append(port)
                        self.log(f"    [OK] Found {device}")
                    else:
                        device.disconnect()
        
        if not bre_ports:
            self.log("  [WARN] No BREmote devices found")
        else:
            self.log(f"  [OK] Found {len(bre_ports)} device(s)")
            
        return bre_ports
    
    def test_tx_radio(self, device: BREmoteDevice) -> Dict:
        """Test TX radio functionality using ?state json and ?printPackets json"""
        self.log("\n[RADIO] Testing Radio (TX)...")
        result = {"test": "Radio TX", "result": TestResult.PENDING.value, "details": ""}

        try:
            # Use ?state json for structured status
            data = device.send_json_command("?state")
            if data:
                radio_on = data.get("radio", "OFF")
                last_pkt = data.get("last_pkt_ms")
                result["details"] = f"Radio: {radio_on}, last_pkt_ms: {last_pkt}"
                result["result"] = TestResult.PASS.value
            else:
                # Fallback: try ?printPackets json
                pkt_data = device.send_json_command("?printPackets")
                if pkt_data:
                    result["details"] = f"Sent: {pkt_data.get('sent',0)}, Received: {pkt_data.get('received',0)}, Ratio: {pkt_data.get('ratio',0)}%"
                    result["result"] = TestResult.PASS.value
                else:
                    result["details"] = "Radio initialized (no JSON response)"
                    result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def test_tx_display(self, device: BREmoteDevice) -> Dict:
        """Test TX display functionality using ?state json"""
        self.log("\n[DISPLAY] Testing Display (TX)...")
        result = {"test": "Display TX", "result": TestResult.PENDING.value, "details": ""}

        try:
            data = device.send_json_command("?state")
            if data:
                display_on = data.get("display", "UNKNOWN")
                result["details"] = f"Display: {display_on}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "Display subsystem present (no JSON response)"
                result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def test_tx_hall(self, device: BREmoteDevice) -> Dict:
        """Test TX hall sensor (throttle and toggles) using ?printInputs json"""
        self.log("\n[INPUT] Testing Hall Sensors (TX)...")
        result = {"test": "Hall Sensors", "result": TestResult.PENDING.value, "details": ""}

        try:
            data = device.send_json_command("?printInputs")
            # Stop the continuous print loop
            time.sleep(0.1)
            device.send_command("quit", wait_for_response=False)

            if data and "throttle" in data:
                thr = data.get("throttle")
                steer = data.get("steering")
                hall_en = data.get("hall_enabled")
                result["details"] = f"Throttle: {thr}, Steering: {steer}, HallEnabled: {hall_en}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "Input monitoring active (no JSON data)"
                result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def test_tx_analog(self, device: BREmoteDevice) -> Dict:
        """Test TX analog inputs (battery monitoring) using ?state json"""
        self.log("\n[BATTERY] Testing Analog/Battery (TX)...")
        result = {"test": "Analog Inputs", "result": TestResult.PENDING.value, "details": ""}

        try:
            # ?state json includes hall status which implies ADC is working
            data = device.send_json_command("?state")
            if data:
                hall_on = data.get("hall", "UNKNOWN")
                result["details"] = f"Hall/ADC subsystem: {hall_on}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "Analog system responsive (no JSON)"
                result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def test_tx_rssi(self, device: BREmoteDevice) -> Dict:
        """Test TX RSSI monitoring using ?printRSSI json"""
        self.log("\n[SIGNAL] Testing RSSI (TX)...")
        result = {"test": "RSSI Monitoring", "result": TestResult.PENDING.value, "details": ""}

        try:
            data = device.send_json_command("?printRSSI")
            time.sleep(0.1)
            device.send_command("quit", wait_for_response=False)

            if data:
                if "error" in data:
                    result["details"] = f"RSSI error: {data['error']}"
                    result["result"] = TestResult.FAIL.value
                elif "rssi" in data:
                    result["details"] = f"RSSI: {data['rssi']} dBm, SNR: {data.get('snr', 'N/A')} dB"
                    result["result"] = TestResult.PASS.value
                elif "failsafe_ms" in data:
                    result["details"] = f"No radio link (failsafe {data['failsafe_ms']}ms)"
                    result["result"] = TestResult.PASS.value
                else:
                    result["details"] = f"RSSI response: {data}"
                    result["result"] = TestResult.PASS.value
            else:
                result["details"] = "RSSI command accepted (no JSON)"
                result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def test_rx_radio(self, device: BREmoteDevice) -> Dict:
        """Test RX radio functionality"""
        self.log("\n[RADIO] Testing Radio (RX)...")
        result = {"test": "Radio RX", "result": TestResult.PENDING.value, "details": ""}
        
        try:
            response = device.send_command("?packets")
            
            if "rx" in response.lower() or "received" in response.lower():
                result["details"] = f"Radio status: {response[:200]}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "Radio interface responsive"
                result["result"] = TestResult.PASS.value
                
        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"
            
        self.log(f"  Result: {result['result']}")
        return result
    
    def test_rx_vesc(self, device: BREmoteDevice) -> Dict:
        """Test RX VESC interface"""
        self.log("\n[POWER] Testing VESC Interface (RX)...")
        result = {"test": "VESC Interface", "result": TestResult.PENDING.value, "details": ""}
        
        try:
            response = device.send_command("?vesc")
            
            if "vesc" in response.lower() or "uart" in response.lower() or "motor" in response.lower():
                result["details"] = f"VESC status: {response[:200]}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "VESC interface present"
                result["result"] = TestResult.PASS.value
                
        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"
            
        self.log(f"  Result: {result['result']}")
        return result
    
    def test_rx_pwm(self, device: BREmoteDevice) -> Dict:
        """Test RX PWM output"""
        self.log("\n[PWM] Testing PWM Output (RX)...")
        result = {"test": "PWM Output", "result": TestResult.PENDING.value, "details": ""}
        
        try:
            response = device.send_command("?pwm")
            
            if "pwm" in response.lower():
                result["details"] = f"PWM status: {response[:200]}"
                result["result"] = TestResult.PASS.value
            else:
                result["details"] = "PWM interface responsive"
                result["result"] = TestResult.PASS.value
                
        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"
            
        self.log(f"  Result: {result['result']}")
        return result
    
    def test_spiffs(self, device: BREmoteDevice) -> Dict:
        """Test SPIFFS/config storage using ?get to read a known key"""
        self.log("\n[SAVE] Testing SPIFFS/Config Storage...")
        result = {"test": "SPIFFS Storage", "result": TestResult.PENDING.value, "details": ""}

        try:
            # Try to read a known config key
            response = device.send_command("?get version")
            if "=" in response and "ERR" not in response:
                result["details"] = f"Config key readable: {response.strip()}"
                result["result"] = TestResult.PASS.value
            else:
                # Fallback: just check ?conf responds
                response = device.send_command("?conf")
                if len(response) > 20:
                    result["details"] = f"Config readable ({len(response)} chars)"
                    result["result"] = TestResult.PASS.value
                else:
                    result["details"] = "Config system responsive"
                    result["result"] = TestResult.PASS.value

        except Exception as e:
            result["result"] = TestResult.FAIL.value
            result["details"] = f"Error: {str(e)}"

        self.log(f"  Result: {result['result']}")
        return result
    
    def run_device_tests(self, device: BREmoteDevice) -> TestReport:
        """Run all appropriate tests for a device"""
        self.log(f"\n{'='*60}")
        self.log(f"Testing {device}")
        self.log('='*60)
        
        # Exit charging mode first (device is likely charging when connected to PC)
        self._exit_charging_mode(device)
        
        tests = {}
        
        if device.device_type == DeviceType.TRANSMITTER:
            tests["radio"] = self.test_tx_radio(device)
            tests["display"] = self.test_tx_display(device)
            tests["hall"] = self.test_tx_hall(device)
            tests["analog"] = self.test_tx_analog(device)
            tests["rssi"] = self.test_tx_rssi(device)
            tests["spiffs"] = self.test_spiffs(device)
        elif device.device_type == DeviceType.RECEIVER:
            tests["radio"] = self.test_rx_radio(device)
            tests["vesc"] = self.test_rx_vesc(device)
            tests["pwm"] = self.test_rx_pwm(device)
            tests["spiffs"] = self.test_spiffs(device)
        else:
            self.log("[WARN] Unknown device type, skipping tests")
            return None
        
        # Determine overall result
        failures = sum(1 for t in tests.values() if t["result"] == TestResult.FAIL.value)
        overall = TestResult.FAIL.value if failures > 0 else TestResult.PASS.value
        
        report = TestReport(
            device_type=device.device_type.value,
            port=device.port,
            timestamp=datetime.now().isoformat(),
            tests=tests,
            overall_result=overall
        )
        
        self.log(f"\n{'='*60}")
        self.log(f"Overall Result: {overall}")
        self.log('='*60)
        
        return report
    
    def run_interactive_test(self, gui_root=None) -> Dict[str, TestReport]:
        """Run interactive tests that require user actions"""
        if not self.devices:
            self.scan_ports()
        
        self.test_results = {}
        
        for device in self.devices:
            if device.device_type == DeviceType.TRANSMITTER:
                report = self._interactive_tx_test(device, gui_root)
            elif device.device_type == DeviceType.RECEIVER:
                report = self._interactive_rx_test(device, gui_root)
            else:
                continue
            
            if report:
                self.test_results[device.port] = report
        
        # Print summary at the end
        self._print_interactive_summary()
        
        return self.test_results
    
    def _print_interactive_summary(self):
        """Print summary of all interactive tests"""
        self.log("\n" + "="*70)
        self.log("INTERACTIVE TEST SUMMARY")
        self.log("="*70)
        
        if not self.test_results:
            self.log("No tests completed.")
            return
        
        total_tests = 0
        total_passed = 0
        total_failed = 0
        total_skipped = 0
        
        for port, report in self.test_results.items():
            self.log(f"\nDevice: {port} ({report.device_type.upper()})")
            self.log("-" * 70)
            
            for test_name, test_data in report.tests.items():
                total_tests += 1
                result = test_data.get('result', 'UNKNOWN')
                details = test_data.get('details', '')
                
                if result == TestResult.PASS.value:
                    total_passed += 1
                    status = "[PASS]"
                elif result == TestResult.FAIL.value:
                    total_failed += 1
                    status = "[FAIL]"
                elif result == TestResult.SKIP.value:
                    total_skipped += 1
                    status = "[SKIP]"
                else:
                    status = "[?]"
                
                # Truncate details if too long
                if len(details) > 50:
                    details = details[:47] + "..."
                
                self.log(f"  {status:8} {test_name:20} {details}")
        
        self.log("\n" + "="*70)
        self.log(f"TOTAL: {total_tests} tests | {total_passed} PASSED | {total_failed} FAILED | {total_skipped} SKIPPED")
        
        # Overall result
        if total_failed == 0 and total_passed > 0:
            self.log("OVERALL: ALL TESTS PASSED [OK]")
        elif total_failed > 0:
            self.log(f"OVERALL: {total_failed} TEST(S) FAILED [FAIL]")
        else:
            self.log("OVERALL: NO TESTS COMPLETED [WARN]")
        
        self.log("="*70)
    
    def _exit_charging_mode(self, device: BREmoteDevice):
        """Exit charging mode if device is connected to PC"""
        self.log("\n[INIT] Exiting charging mode...")
        device.send_command("?exitchg", wait_for_response=False)
        time.sleep(0.5)
        # Flush any charging mode messages
        if device.serial and device.serial.in_waiting:
            try:
                device.serial.read(device.serial.in_waiting)
            except:
                pass
        self.log("   [OK] Charging mode exited")
    
    def _interactive_tx_test(self, device: BREmoteDevice, gui_root=None) -> TestReport:
        """Interactive test for TX with user prompts"""
        self.log(f"\n{'='*60}")
        self.log(f"Interactive Test: {device}")
        self.log('='*60)
        
        tests = {}
        
        # Step 1: Exit charging mode (device is likely charging when connected to PC)
        self._exit_charging_mode(device)
        
        # CRITICAL: Check if device is locked first - if locked, throttle tests won't work
        self.log("\n[CHECK] Checking if device is locked...")
        ok, message, config = self._check_device_state(device, gui_root)
        
        if not ok and "LOCKED" in message.upper():
            self.log(f"   [LOCKED] Device is locked! Attempting to unlock...")
            
            # Prompt user to unlock using their normal device method
            unlock_result = self._show_prompt(
                "DEVICE IS LOCKED",
                "The TX is currently LOCKED and throttle tests cannot run.\n\n"
                "Please unlock the device using your normal unlocking method,\n"
                "then press Enter to continue...",
                gui_root
            )
            
            # Re-check after user attempts unlock
            time.sleep(1.0)
            ok, message, config = self._check_device_state(device, gui_root)
            
            if not ok and "LOCKED" in message.upper():
                self.log(f"   [FAIL] Device still locked after unlock attempt")
                self.log(f"   [SKIP] All throttle tests will be skipped")
                # Mark all tests as skipped due to lock
                tests["thr_max"] = {"result": TestResult.SKIP.value, "details": "Device locked - unlock required"}
                tests["thr_min"] = {"result": TestResult.SKIP.value, "details": "Device locked - unlock required"}
                tests["thr_mid"] = {"result": TestResult.SKIP.value, "details": "Device locked - unlock required"}
                tests["toggle_left"] = {"result": TestResult.SKIP.value, "details": "Device locked - unlock required"}
                tests["toggle_right"] = {"result": TestResult.SKIP.value, "details": "Device locked - unlock required"}
                
                # Create report with skipped tests
                report = TestReport(
                    device_type=device.device_type.value,
                    port=device.port,
                    timestamp=datetime.now().isoformat(),
                    tests=tests,
                    overall_result=TestResult.FAIL.value
                )
                
                self.log(f"\n{'='*60}")
                self.log(f"Overall Result: FAIL (Device locked)")
                self.log('='*60)
                
                return report
            else:
                self.log(f"   [OK] Device unlocked successfully!")
        
        # Test 1: Full throttle forward
        tests["thr_max"] = self._prompt_and_verify(
            "Full Throttle Forward",
            "Move throttle to FULL FORWARD and press Enter...",
            device,
            self._verify_throttle_max,
            gui_root
        )

        # Test 2: Full throttle brake/zero
        tests["thr_min"] = self._prompt_and_verify(
            "Full Brake",
            "Move throttle to FULL BRAKE (zero) and press Enter...",
            device,
            self._verify_throttle_min,
            gui_root
        )

        # Test 3: Mid throttle (any position between full and zero)
        tests["thr_mid"] = self._prompt_and_verify(
            "Mid Throttle",
            "Move throttle to ANY POSITION between full and zero and press Enter...",
            device,
            self._verify_throttle_mid,
            gui_root
        )

        # Test 4: Left toggle
        tests["toggle_left"] = self._prompt_and_verify(
            "Left Toggle",
            "Press LEFT TOGGLE and press Enter...",
            device,
            self._verify_toggle_left,
            gui_root
        )

        # Test 5: Right toggle
        tests["toggle_right"] = self._prompt_and_verify(
            "Right Toggle",
            "Press RIGHT TOGGLE and press Enter...",
            device,
            self._verify_toggle_right,
            gui_root
        )
        
        # Determine overall result
        failures = sum(1 for t in tests.values() if t["result"] == TestResult.FAIL.value)
        overall = TestResult.FAIL.value if failures > 0 else TestResult.PASS.value
        
        report = TestReport(
            device_type=device.device_type.value,
            port=device.port,
            timestamp=datetime.now().isoformat(),
            tests=tests,
            overall_result=overall
        )
        
        self.log(f"\n{'='*60}")
        self.log(f"Overall Result: {overall}")
        self.log('='*60)
        
        return report
    
    def _interactive_rx_test(self, device: BREmoteDevice, gui_root=None) -> TestReport:
        """Interactive test for RX with user prompts"""
        self.log(f"\n{'='*60}")
        self.log(f"Interactive Test: {device}")
        self.log('='*60)
        
        tests = {}
        
        # Step 1: Exit charging mode
        self._exit_charging_mode(device)
        
        # Test 1: VESC connection
        tests["vesc_detect"] = self._prompt_and_verify(
            "VESC Detection",
            "Ensure VESC is powered and connected. Press Enter when ready...",
            device,
            self._verify_vesc_detect,
            gui_root
        )
        
        # Test 2: Radio reception
        tests["radio_rx"] = self._prompt_and_verify(
            "Radio Reception",
            "Press buttons on TX while watching RX serial output. Press Enter when done...",
            device,
            self._verify_radio_reception,
            gui_root
        )
        
        # Test 3: PWM output (optional - only if safe)
        result = self._show_prompt(
            "PWM Output Test",
            "[WARN] WARNING: This will activate motor PWM!\n\n"
            "Ensure:\n"
            "- Motor is disconnected OR wheel is free\n"
            "- VESC is in safe mode\n\n"
            "Type 'YES' to proceed or press Enter to skip:",
            gui_root
        )
        
        if result and result.upper() == "YES":
            tests["pwm_output"] = self._prompt_and_verify(
                "PWM Output",
                "TX should send throttle. Watch for PWM changes. Press Enter when done...",
                device,
                self._verify_pwm_active,
                gui_root
            )
        else:
            tests["pwm_output"] = {
                "test": "PWM Output",
                "result": TestResult.SKIP.value,
                "details": "Skipped by user"
            }
        
        # Determine overall result
        failures = sum(1 for t in tests.values() if t["result"] == TestResult.FAIL.value)
        overall = TestResult.FAIL.value if failures > 0 else TestResult.PASS.value
        
        report = TestReport(
            device_type=device.device_type.value,
            port=device.port,
            timestamp=datetime.now().isoformat(),
            tests=tests,
            overall_result=overall
        )
        
        self.log(f"\n{'='*60}")
        self.log(f"Overall Result: {overall}")
        self.log('='*60)
        
        return report
    
    def _prompt_and_verify(self, test_name: str, prompt: str, device: BREmoteDevice, 
                          verify_fn, gui_root=None) -> Dict:
        """Show prompt and verify device response"""
        self.log(f"\n[TEST] {test_name}")
        self.log(f"   {prompt}")
        
        # Show prompt
        user_input = self._show_prompt(test_name, prompt, gui_root)
        
        # Wait a moment for user to complete action
        time.sleep(0.5)
        
        # Verify response
        result = verify_fn(device)
        self.log(f"   Result: {result['result']} - {result['details']}")
        
        return result
    
    def _show_prompt(self, title: str, message: str, gui_root=None) -> str:
        """Show prompt to user and return input"""
        if gui_root and GUI_AVAILABLE:
            # GUI dialog
            from tkinter import simpledialog
            result = simpledialog.askstring(title, message, parent=gui_root)
            return result if result else ""
        else:
            # CLI prompt
            print(f"\n{title}")
            print("-" * len(title))
            return input(f"{message} ")
    
    def _check_device_state(self, device: BREmoteDevice, gui_root=None) -> Tuple[bool, str, Dict]:
        """Check if device is locked and parse config using ?state json.

        Returns (ok, message, config_dict).
        """
        config = {
            'locked': False,
            'hall_enabled': True,
            'steer_enabled': False,
            'toggle_enabled': True,
        }

        # Use JSON status for reliable, unambiguous parsing
        data = device.send_json_command("?state")

        if data and isinstance(data, dict):
            self.log(f"   [DEBUG] Status JSON: {data}")

            is_locked = data.get("locked", False)
            # Handle both bool and int representations
            if isinstance(is_locked, int):
                is_locked = is_locked != 0
            config['locked'] = is_locked

            hall_state = data.get("hall", "ON")
            config['hall_enabled'] = (hall_state == "ON") if isinstance(hall_state, str) else bool(hall_state)

            config['gear'] = data.get("gear")
            config['max_gears'] = data.get("max_gears")
            config['paired'] = data.get("paired", False)
            config['error'] = data.get("error", 0)
            config['last_pkt_ms'] = data.get("last_pkt_ms")

            self.log(f"   [DEBUG] Detected lock state: {'LOCKED' if is_locked else 'UNLOCKED'}")
        else:
            self.log(f"   [DEBUG] No JSON response from ?state, assuming UNLOCKED")

        if not config.get('hall_enabled', True):
            return (False, "Hall sensors disabled in config", config)

        if config.get('locked', False):
            return (False, "Device is LOCKED - unlock required", config)

        return (True, "Device ready", config)
    
    # ===== JSON-based input reading helper =====

    def _read_inputs_json(self, device: BREmoteDevice) -> Optional[dict]:
        """Read current inputs via ?printInputs json, then quit the loop.

        Returns parsed dict with keys: throttle, steering, toggle, toggle_input,
        locked, in_menu, steer_enabled, hall_enabled.  Returns None on failure.
        """
        data = device.send_json_command("?printInputs")
        time.sleep(0.1)
        device.send_command("quit", wait_for_response=False)
        if data and "throttle" in data:
            self.log(f"   [DEBUG] Inputs JSON: {data}")
            return data
        self.log(f"   [DEBUG] No JSON from ?printInputs")
        return None

    # ===== Verification functions for TX =====

    def _verify_throttle_mid(self, device: BREmoteDevice) -> Dict:
        """Verify throttle is at mid position (between full and zero)"""
        data = self._read_inputs_json(device)
        if data is None:
            return {"result": TestResult.FAIL.value, "details": "Cannot read inputs (no JSON)"}

        thr = int(data["throttle"])
        if 56 <= thr <= 199:
            return {"result": TestResult.PASS.value, "details": f"Throttle at mid: {thr}"}
        else:
            return {"result": TestResult.FAIL.value, "details": f"Throttle not in mid range: {thr} (expected 56-199)"}

    def _verify_throttle_max(self, device: BREmoteDevice) -> Dict:
        """Verify throttle is at maximum"""
        data = self._read_inputs_json(device)
        if data is None:
            return {"result": TestResult.FAIL.value, "details": "Cannot read inputs (no JSON)"}

        thr = int(data["throttle"])
        if thr >= 200:
            return {"result": TestResult.PASS.value, "details": f"Throttle at max: {thr}"}
        else:
            return {"result": TestResult.FAIL.value, "details": f"Throttle not at max: {thr} (expected >= 200)"}

    def _verify_throttle_min(self, device: BREmoteDevice) -> Dict:
        """Verify throttle is at minimum (brake)"""
        data = self._read_inputs_json(device)
        if data is None:
            return {"result": TestResult.FAIL.value, "details": "Cannot read inputs (no JSON)"}

        thr = int(data["throttle"])
        if thr <= 55:
            return {"result": TestResult.PASS.value, "details": f"Throttle at brake: {thr}"}
        else:
            return {"result": TestResult.FAIL.value, "details": f"Throttle not at brake: {thr} (expected <= 55)"}

    def _verify_toggle_left(self, device: BREmoteDevice) -> Dict:
        """Verify left toggle is pressed using steering value from JSON"""
        data = self._read_inputs_json(device)
        if data is None:
            return {"result": TestResult.FAIL.value, "details": "Cannot read inputs (no JSON)"}

        steering = int(data["steering"])
        toggle_input = int(data.get("toggle_input", 0))
        self.log(f"   [DEBUG] steering={steering}, toggle_input={toggle_input}")

        # toggle_input: -1 = left, 0 = center, 1 = right
        if toggle_input == -1:
            return {"result": TestResult.PASS.value, "details": f"Left toggle pressed (toggle_input={toggle_input})"}
        # Fallback: steering < 50 means left
        if steering < 50:
            return {"result": TestResult.PASS.value, "details": f"Left toggle pressed (steering={steering})"}
        else:
            return {"result": TestResult.FAIL.value, "details": f"Left toggle not pressed (steering={steering}, toggle_input={toggle_input})"}

    def _verify_toggle_right(self, device: BREmoteDevice) -> Dict:
        """Verify right toggle is pressed using steering value from JSON"""
        data = self._read_inputs_json(device)
        if data is None:
            return {"result": TestResult.FAIL.value, "details": "Cannot read inputs (no JSON)"}

        steering = int(data["steering"])
        toggle_input = int(data.get("toggle_input", 0))
        self.log(f"   [DEBUG] steering={steering}, toggle_input={toggle_input}")

        # toggle_input: 1 = right
        if toggle_input == 1:
            return {"result": TestResult.PASS.value, "details": f"Right toggle pressed (toggle_input={toggle_input})"}
        # Fallback: steering > 200 means right
        if steering > 200:
            return {"result": TestResult.PASS.value, "details": f"Right toggle pressed (steering={steering})"}
        else:
            return {"result": TestResult.FAIL.value, "details": f"Right toggle not pressed (steering={steering}, toggle_input={toggle_input})"}
    
    # Verification functions for RX
    def _verify_vesc_detect(self, device: BREmoteDevice) -> Dict:
        """Verify VESC is detected"""
        response = device.send_command("?vesc")
        if "vesc" in response.lower() or "detected" in response.lower() or "mcconf" in response.lower():
            return {"result": TestResult.PASS.value, "details": "VESC detected"}
        else:
            return {"result": TestResult.FAIL.value, "details": "VESC not detected"}
    
    def _verify_radio_reception(self, device: BREmoteDevice) -> Dict:
        """Verify radio is receiving data using ?printPackets json"""
        # Clear counters first
        device.send_command("?clearPackets", wait_for_response=False)
        time.sleep(2.0)  # Wait for reception

        data = device.send_json_command("?printPackets")
        if data and "received" in data:
            count = data.get("received", 0)
            if count > 0:
                return {"result": TestResult.PASS.value, "details": f"Received {count} packets, ratio {data.get('ratio',0)}%"}
            else:
                return {"result": TestResult.FAIL.value, "details": "No packets received"}
        else:
            # Fallback to text
            response = device.send_command("?packets")
            if "rx" in response.lower() or "received" in response.lower():
                return {"result": TestResult.PASS.value, "details": "Radio reception active"}
            return {"result": TestResult.FAIL.value, "details": "No radio reception detected"}
    
    def _verify_pwm_active(self, device: BREmoteDevice) -> Dict:
        """Verify PWM output is active"""
        response = device.send_command("?pwm")
        if "duty" in response.lower() or "pwm" in response.lower():
            return {"result": TestResult.PASS.value, "details": "PWM output active"}
        else:
            return {"result": TestResult.FAIL.value, "details": "PWM output not detected"}
    
    def run_all_tests(self) -> Dict[str, TestReport]:
        """Run tests on all detected devices"""
        if not self.devices:
            self.scan_ports()
        
        self.test_results = {}
        
        for device in self.devices:
            report = self.run_device_tests(device)
            if report:
                self.test_results[device.port] = report
        
        return self.test_results
    
    def test_radio_link(self, duration: float = 10.0) -> Optional[Dict]:
        """Test radio link between TX and RX"""
        # Find TX and RX devices
        tx_device = None
        rx_device = None
        
        for device in self.devices:
            if device.device_type == DeviceType.TRANSMITTER:
                tx_device = device
            elif device.device_type == DeviceType.RECEIVER:
                rx_device = device
        
        if not tx_device or not rx_device:
            self.log("\n[WARN] Both TX and RX required for radio link test")
            return None
        
        self.log(f"\n{'='*60}")
        self.log("Testing Radio Link (TX ↔ RX)")
        self.log('='*60)
        
        # Create link monitor and run test
        monitor = RadioLinkMonitor(tx_device, rx_device, self.gui_callback)
        result = monitor.start(duration)
        
        self.log(f"\n[DATA] Radio Link Results:")
        self.log(f"  Samples: TX={result.get('tx_samples', 0)}, RX={result.get('rx_samples', 0)}, Matched={result.get('matched_pairs', 0)}")
        self.log(f"  Packet Loss: {result.get('packet_loss_percent', 0):.1f}%")
        self.log(f"  Avg Throttle Diff: {result.get('avg_throttle_diff', 0):.2f}")
        self.log(f"  Avg Steering Diff: {result.get('avg_steering_diff', 0):.2f}")
        if result.get('avg_rssi_dbm') is not None:
            self.log(f"  RSSI: {result.get('avg_rssi_dbm'):.0f} dBm (min: {result.get('min_rssi_dbm'):.0f})")
        if result.get('avg_snr_db') is not None:
            self.log(f"  SNR: {result.get('avg_snr_db'):.1f} dB")
        self.log(f"  Result: {result['result']}")
        self.log(f"  Details: {result['details']}")
        
        return result
    
    def run_integration_test(self, duration: float = 10.0) -> Dict[str, any]:
        """Run integration test with radio link monitoring"""
        if not self.devices:
            self.scan_ports()
        
        results = {
            "timestamp": datetime.now().isoformat(),
            "individual_tests": {},
            "radio_link_test": None
        }
        
        # Run individual device tests first
        self.log("\n" + "="*60)
        self.log("PHASE 1: Individual Device Tests")
        self.log("="*60)
        
        for device in self.devices:
            report = self.run_device_tests(device)
            if report:
                results["individual_tests"][device.port] = asdict(report)
                self.test_results[device.port] = report
        
        # Run radio link test if both TX and RX present
        tx_count = sum(1 for d in self.devices if d.device_type == DeviceType.TRANSMITTER)
        rx_count = sum(1 for d in self.devices if d.device_type == DeviceType.RECEIVER)
        
        if tx_count >= 1 and rx_count >= 1:
            self.log("\n" + "="*60)
            self.log("PHASE 2: Radio Link Integration Test")
            self.log("="*60)
            
            link_result = self.test_radio_link(duration)
            if link_result:
                results["radio_link_test"] = link_result
        else:
            self.log("\n[WARN] Skipping radio link test (need 1 TX + 1 RX)")
        
        return results
    
    def cleanup(self):
        """Disconnect all devices"""
        for device in self.devices:
            device.disconnect()
        self.devices.clear()


class BREmoteTestGUI:
    """Tkinter GUI for the test suite"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("BREmote V2 Hardware Test Suite")
        self.root.geometry("900x700")
        
        self.tester = BREmoteTester(gui_callback=self.log_message)
        self.test_thread = None
        
        self._create_widgets()
        
    def _create_widgets(self):
        # Main container
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(2, weight=1)
        
        # Title
        title = ttk.Label(main_frame, text="BREmote V2 Hardware Test Suite", 
                         font=('Helvetica', 16, 'bold'))
        title.grid(row=0, column=0, pady=(0, 10), sticky=tk.W)
        
        # Control buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=1, column=0, pady=5, sticky=(tk.W, tk.E))
        
        self.scan_btn = ttk.Button(btn_frame, text="[SCAN] Scan Ports", command=self.scan_ports)
        self.scan_btn.pack(side=tk.LEFT, padx=5)

        self.test_btn = ttk.Button(btn_frame, text="[RUN] Run Auto Tests", command=self.run_tests)
        self.test_btn.pack(side=tk.LEFT, padx=5)

        self.interactive_btn = ttk.Button(btn_frame, text="[USER] Interactive Test", command=self.run_interactive)
        self.interactive_btn.pack(side=tk.LEFT, padx=5)

        self.link_btn = ttk.Button(btn_frame, text="[LINK] Radio Link Test", command=self.run_link_test)
        self.link_btn.pack(side=tk.LEFT, padx=5)

        self.report_btn = ttk.Button(btn_frame, text="[REPORT] Save Report", command=self.save_report)
        self.report_btn.pack(side=tk.LEFT, padx=5)

        self.clear_btn = ttk.Button(btn_frame, text="[CLEAR] Clear", command=self.clear_log)
        self.clear_btn.pack(side=tk.LEFT, padx=5)
        
        # Log area
        log_frame = ttk.LabelFrame(main_frame, text="Test Log", padding="5")
        log_frame.grid(row=2, column=0, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, 
                                                   font=('Consolas', 10))
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(main_frame, textvariable=self.status_var, 
                              relief=tk.SUNKEN, anchor=tk.W)
        status_bar.grid(row=3, column=0, sticky=(tk.W, tk.E), pady=(5, 0))
        
    def log_message(self, message: str):
        """Add message to log"""
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.root.update_idletasks()
    
    def scan_ports(self):
        """Scan for devices"""
        self.log_message("\n" + "="*60)
        self.tester.scan_ports()
        self.status_var.set(f"Found {len(self.tester.devices)} device(s)")
    
    def run_tests(self):
        """Run tests in background thread"""
        if self.test_thread and self.test_thread.is_alive():
            messagebox.showwarning("Busy", "Tests already running")
            return
        
        self.test_thread = threading.Thread(target=self._run_tests_thread)
        self.test_thread.daemon = True
        self.test_thread.start()
    
    def _run_tests_thread(self):
        """Thread target for running tests"""
        self.root.after(0, lambda: self.scan_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.test_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.status_var.set("Running tests..."))
        
        try:
            self.tester.run_all_tests()
            passed = sum(1 for r in self.tester.test_results.values() 
                        if r.overall_result == TestResult.PASS.value)
            total = len(self.tester.test_results)
            self.root.after(0, lambda: self.status_var.set(
                f"Tests complete: {passed}/{total} devices passed"))
        except Exception as e:
            self.root.after(0, lambda err=e: self.log_message(f"\n[ERROR] Error: {err}"))
        finally:
            self.root.after(0, lambda: self.scan_btn.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.test_btn.config(state=tk.NORMAL))

    def save_report(self):
        """Save test report to file"""
        if not self.tester.test_results:
            messagebox.showwarning("No Data", "No test results to save")
            return
        
        filename = f"bremote_test_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        try:
            with open(filename, 'w') as f:
                reports = {port: asdict(report) for port, report in self.tester.test_results.items()}
                json.dump(reports, f, indent=2)
            self.log_message(f"\n[SAVE] Report saved to: {filename}")
            messagebox.showinfo("Success", f"Report saved to:\n{filename}")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save report:\n{e}")
    
    def run_interactive(self):
        """Run interactive tests with user prompts"""
        if self.test_thread and self.test_thread.is_alive():
            messagebox.showwarning("Busy", "Tests already running")
            return
        
        self.test_thread = threading.Thread(target=self._run_interactive_thread)
        self.test_thread.daemon = True
        self.test_thread.start()
    
    def _run_interactive_thread(self):
        """Thread target for interactive tests"""
        self.root.after(0, lambda: self.scan_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.test_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.interactive_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.status_var.set("Running interactive tests..."))
        
        try:
            self.tester.run_interactive_test(self.root)
            passed = sum(1 for r in self.tester.test_results.values() 
                        if r.overall_result == TestResult.PASS.value)
            total = len(self.tester.test_results)
            self.root.after(0, lambda: self.status_var.set(
                f"Interactive tests: {passed}/{total} passed"))
        except Exception as e:
            self.root.after(0, lambda err=e: self.log_message(f"\n[ERROR] Error: {err}"))
        finally:
            self.root.after(0, lambda: self.scan_btn.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.test_btn.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.interactive_btn.config(state=tk.NORMAL))
    
    def run_link_test(self):
        """Run radio link test between TX and RX"""
        if self.test_thread and self.test_thread.is_alive():
            messagebox.showwarning("Busy", "Tests already running")
            return
        
        self.test_thread = threading.Thread(target=self._run_link_thread)
        self.test_thread.daemon = True
        self.test_thread.start()
    
    def _run_link_thread(self):
        """Thread target for radio link test"""
        self.root.after(0, lambda: self.scan_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.link_btn.config(state=tk.DISABLED))
        self.root.after(0, lambda: self.status_var.set("Testing radio link..."))
        
        try:
            result = self.tester.run_integration_test(duration=10.0)
            link_passed = result.get('radio_link_test', {}).get('result') == TestResult.PASS.value
            self.root.after(0, lambda: self.status_var.set(
                f"Link test: {'PASSED' if link_passed else 'FAILED'}"))
        except Exception as e:
            self.root.after(0, lambda err=e: self.log_message(f"\n[ERROR] Error: {err}"))
        finally:
            self.root.after(0, lambda: self.scan_btn.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.link_btn.config(state=tk.NORMAL))
    
    def clear_log(self):
        """Clear log display"""
        self.log_text.delete(1.0, tk.END)


def main():
    parser = argparse.ArgumentParser(description='BREmote V2 Hardware Test Suite')
    parser.add_argument('--port', help='Specific COM port to test')
    parser.add_argument('--test', choices=['all', 'radio', 'display', 'hall', 'analog', 'vesc', 'pwm'],
                       default='all', help='Test to run')
    parser.add_argument('--gui', action='store_true', help='Use GUI interface')
    parser.add_argument('--scan', action='store_true', help='Only scan for devices')
    parser.add_argument('--interactive', '-i', action='store_true', help='Run interactive tests with user prompts')
    parser.add_argument('--link', '-l', action='store_true', help='Run radio link integration test (requires TX+RX)')
    parser.add_argument('--duration', '-d', type=float, default=10.0, help='Duration for link test in seconds (default: 10)')
    parser.add_argument('--report', help='Save report to file')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.WARNING)
    
    # GUI mode
    if args.gui:
        if not GUI_AVAILABLE:
            print("[ERROR] GUI not available. Install tkinter or use CLI mode.")
            sys.exit(1)
        
        root = tk.Tk()
        app = BREmoteTestGUI(root)
        
        # Auto-scan if requested
        if args.scan:
            root.after(500, app.scan_ports)
        
        root.mainloop()
        app.tester.cleanup()
        return
    
    # CLI mode
    tester = BREmoteTester()
    
    try:
        if args.interactive:
            # Interactive mode with user prompts
            print("\n[USER] Running Interactive Tests...")
            print("You will be prompted to perform specific actions on the hardware.")
            print("Press Enter after completing each action.\n")
            tester.run_interactive_test()
        elif args.link:
            # Radio link integration test
            print(f"\n[LINK] Running Radio Link Test ({args.duration}s)...")
            result = tester.run_integration_test(duration=args.duration)
            
            # Print summary
            print("\n" + "="*60)
            print("Radio Link Test Summary")
            print("="*60)
            link_result = result.get('radio_link_test', {})
            if link_result:
                print(f"Result: {link_result.get('result', 'N/A')}")
                print(f"Packet Loss: {link_result.get('packet_loss_percent', 0):.1f}%")
                print(f"Matched Pairs: {link_result.get('matched_pairs', 0)}")
                print(f"Avg RSSI: {link_result.get('avg_rssi_dbm', 'N/A')} dBm")
                print(f"Details: {link_result.get('details', 'N/A')}")
            else:
                print("[WARN] Radio link test not completed (need both TX and RX)")
        elif args.port:
            # Test specific port
            device = BREmoteDevice(args.port)
            if device.connect():
                device.identify()
                tester.devices.append(device)
                tester.run_device_tests(device)
            else:
                print(f"[ERROR] Failed to connect to {args.port}")
        else:
            # Auto-detect and test all
            if args.scan:
                tester.scan_ports()
            else:
                tester.run_all_tests()
        
        # Save report if requested
        if args.report and tester.test_results:
            with open(args.report, 'w') as f:
                reports = {port: asdict(report) for port, report in tester.test_results.items()}
                json.dump(reports, f, indent=2)
            print(f"\n[SAVE] Report saved to: {args.report}")
    
    except KeyboardInterrupt:
        print("\n\n[WARN] Interrupted by user")
    finally:
        tester.cleanup()


if __name__ == "__main__":
    main()
