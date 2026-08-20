#!/usr/bin/env python3
"""Flash a benchmark image onto an attached Pico and capture what it prints.

    ./run.py ../../build-pico/O2/yax86_pico_bench.uf2

The board is rebooted into BOOTSEL with picotool rather than by hand, so a run
needs no button press:

    picotool reboot -u -f && sleep 3 && picotool load -x <image>.uf2

The serial port must then be opened exactly once. The firmware waits for DTR
before it prints anything, and every open asserts it - so configuring the line
with a separate stty first starts the run, and by the time a reader attached
the header would already have scrolled past. That is why the line is
configured with termios on the descriptor this keeps.
"""

import argparse
import glob
import os
import select
import subprocess
import sys
import termios
import time

# How long to give the board to re-enumerate, in seconds. It comes back as a
# USB mass storage device after the reboot and as a CDC port after the load,
# and picotool fails outright if it is asked too early.
ENUMERATE_DELAY = 3
# How long to wait for a serial port to appear, and how long to let it settle
# once it has. Enumeration creates the node slightly before it is usable.
PORT_WAIT = 25
PORT_SETTLE = 0.8


def flash(uf2):
    """Reboot the board into BOOTSEL and load an image onto it."""
    # A board already in BOOTSEL has no serial connection to reboot through, so
    # a failure here is not fatal - the load below is what has to work.
    subprocess.run(
        ["picotool", "reboot", "-u", "-f"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(ENUMERATE_DELAY)
    load = subprocess.run(
        ["picotool", "load", "-x", uf2],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    if load.returncode != 0:
        sys.stderr.write(load.stderr.decode("utf8", "replace"))
        raise SystemExit("picotool load failed")


def wait_for_port(timeout=PORT_WAIT):
    """Find the board's CDC port.

    Not hard-coded: the port re-enumerates after every flash and after a
    watchdog reset, and can come back on a different node than it left on.
    """
    end = time.time() + timeout
    while time.time() < end:
        ports = sorted(glob.glob("/dev/ttyACM*"))
        if ports:
            time.sleep(PORT_SETTLE)
            return ports[-1]
        time.sleep(0.2)
    return None


def capture(port, budget):
    """Read from the port until the firmware says it is done, or time runs out.

    Returns the captured text, which is echoed as it arrives so that a long
    run can be watched.
    """
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(fd)
        # Raw: no translation of any kind in either direction, and no line
        # discipline. The baud rate is ignored by a CDC port.
        termios.tcsetattr(
            fd,
            termios.TCSANOW,
            [0, 0, termios.CS8 | termios.CREAD | termios.CLOCAL, 0,
             ispeed, ospeed, cc],
        )
        out, end = "", time.time() + budget
        while time.time() < end:
            readable, _, _ = select.select([fd], [], [], 0.5)
            if readable:
                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    chunk = b""
                if chunk:
                    # The SDK's stdio ends every line with CRLF, so the
                    # newlines are normalized here rather than left for every
                    # reader of the capture to cope with.
                    text = chunk.decode("utf8", "replace")
                    text = text.replace("\r\n", "\n")
                    out += text
                    sys.stdout.write(text)
                    sys.stdout.flush()
            if "\ndone\n" in out:
                return out
        sys.stdout.write(
            "\n[timed out after %.0fs; %d bytes]\n" % (budget, len(out)))
        return out
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uf2", help="image to flash")
    parser.add_argument(
        "--timeout", type=float, default=180.0,
        help="seconds to wait for the run to finish (default: 180)")
    parser.add_argument(
        "--out", help="also write the captured output to this file")
    parser.add_argument(
        "--no-flash", action="store_true",
        help="capture from a board that is already running the image")
    args = parser.parse_args()

    if not args.no_flash:
        flash(args.uf2)
    port = wait_for_port()
    if port is None:
        raise SystemExit("no /dev/ttyACM* appeared")
    out = capture(port, args.timeout)
    if args.out:
        with open(args.out, "w") as f:
            f.write(out)
    return 0 if "\ndone\n" in out else 1


if __name__ == "__main__":
    sys.exit(main())
