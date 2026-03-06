"""
BREmote Test Suite - Main Entry Point
Command-line interface for running tests.
"""

import sys
import argparse
import json
from dataclasses import asdict

from . import BREmoteTester, TestResult


def main():
    parser = argparse.ArgumentParser(description='BREmote V2 Hardware Test Suite')
    parser.add_argument('--port', help='Specific COM port to test')
    parser.add_argument('--test', choices=['all', 'radio', 'display', 'hall', 'analog', 'vesc', 'pwm'],
                       default='all', help='Test to run')
    parser.add_argument('--scan', action='store_true', help='Only scan for devices')
    parser.add_argument('--interactive', '-i', action='store_true', help='Run interactive tests')
    parser.add_argument('--wifi', '-w', action='store_true', help='Run web config / WiFi tests')
    parser.add_argument('--link', '-l', action='store_true', help='Run radio link test (requires TX+RX)')
    parser.add_argument('--duration', '-d', type=float, default=10.0, help='Link test duration in seconds')
    parser.add_argument('--report', help='Save report to JSON file')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    tester = BREmoteTester()
    
    try:
        if args.wifi:
            print("\n[WIFI] Running Web Config / WiFi Tests...")
            tester.run_wifi_tests()
        elif args.link:
            result = tester.run_integration_test(duration=args.duration)
            link = result.get('radio_link_test', {})
            if link:
                print(f"Result: {link.get('result', 'N/A')}")
                print(f"Packet Loss: {link.get('packet_loss_percent', 0):.1f}%")
                print(f"Matched Pairs: {link.get('matched_pairs', 0)}")
                details = link.get('details', '')
                if details and 'working correctly' not in details:
                    print(f"Details: {details}")
        elif args.interactive:
            print("\n[INTERACTIVE] Running Interactive Tests...")
            tester.run_interactive()
        elif args.port:
            from . import BREmoteDevice
            device = BREmoteDevice(args.port)
            if device.connect():
                device.identify()
                tester.devices.append(device)
                tester.run_device_tests(device)
            else:
                print(f"[ERROR] Failed to connect to {args.port}")
        elif args.scan:
            tester.scan_ports()
        else:
            tester.run_all_tests()
        
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
