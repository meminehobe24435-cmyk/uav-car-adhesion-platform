#!/usr/bin/env python3
"""Send the first-stage flight-controller-to-car UART protocol."""

from __future__ import annotations

import argparse
import time


HEADER = bytes((0xAA, 0x55))
VERSION = 0x01
BAUDRATE = 115200

COMMANDS = {
    "stop": (0, 0, 0, 1),
    "forward": (200, 0, 0, 1),
    "backward": (-200, 0, 0, 1),
    "left": (200, 300, 0, 1),
    "right": (200, -300, 0, 1),
    "crab_left": (200, 300, 1, 1),
    "crab_right": (200, -300, 1, 1),
    "enable": (0, 0, 0, 1),
    "disable": (0, 0, 0, 0),
}


def crc8_atm(data: bytes | bytearray) -> int:
    """CRC-8/ATM: poly=0x07, init=0x00, refin=false, xorout=0x00."""
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def build_frame(
    throttle: int,
    steering: int,
    mode: int,
    enable: int,
    sequence: int,
) -> bytes:
    if not -1000 <= throttle <= 1000:
        raise ValueError("throttle must be in -1000..1000")
    if not -1000 <= steering <= 1000:
        raise ValueError("steering must be in -1000..1000")
    if mode not in (0, 1):
        raise ValueError("mode must be 0 or 1")
    if enable not in (0, 1):
        raise ValueError("enable must be 0 or 1")

    frame = bytearray(HEADER)
    frame.extend((VERSION, enable, mode))
    frame.extend(throttle.to_bytes(2, byteorder="big", signed=True))
    frame.extend(steering.to_bytes(2, byteorder="big", signed=True))
    frame.append(sequence & 0xFF)
    frame.append(crc8_atm(frame))
    return bytes(frame)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send 11-byte car commands at a 100 ms heartbeat interval."
    )
    parser.add_argument("port", help="USB-TTL serial port, for example COM5")
    parser.add_argument("command", choices=sorted(COMMANDS))
    parser.add_argument(
        "--interval",
        type=float,
        default=0.1,
        help="heartbeat interval in seconds (default: 0.1)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="seconds to send; 0 means until Ctrl+C (default: 0)",
    )
    parser.add_argument(
        "--no-final-disable",
        action="store_true",
        help="do not send three disable frames on exit; useful for timeout testing",
    )
    return parser.parse_args()


def send_disable(serial_port, sequence: int) -> int:
    for _ in range(3):
        serial_port.write(build_frame(0, 0, 0, 0, sequence))
        serial_port.flush()
        sequence = (sequence + 1) & 0xFF
        time.sleep(0.05)
    return sequence


def main() -> int:
    args = parse_args()
    if args.interval <= 0:
        raise SystemExit("--interval must be greater than zero")
    if args.duration < 0:
        raise SystemExit("--duration cannot be negative")

    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial is required: python -m pip install pyserial") from exc

    throttle, steering, mode, enable = COMMANDS[args.command]
    sequence = 0
    sent = 0
    started = time.monotonic()

    with serial.Serial(args.port, BAUDRATE, bytesize=8, parity="N", stopbits=1) as port:
        print(
            f"Sending {args.command} on {args.port} at {BAUDRATE} 8N1; "
            f"throttle={throttle}, steering={steering}, mode={mode}, enable={enable}"
        )
        try:
            while args.duration == 0.0 or time.monotonic() - started < args.duration:
                frame = build_frame(throttle, steering, mode, enable, sequence)
                port.write(frame)
                port.flush()
                if sent == 0:
                    print("First frame:", frame.hex(" ").upper())
                sent += 1
                sequence = (sequence + 1) & 0xFF
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\nTransmission interrupted.")
        finally:
            if not args.no_final_disable:
                sequence = send_disable(port, sequence)
                print("Sent three final disable frames.")

    print(f"Sent {sent} command frame(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
