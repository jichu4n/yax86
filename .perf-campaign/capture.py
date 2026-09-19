# Capture a run from the Pico harness.
#
# The port must be opened exactly once: the firmware waits on DTR before it
# prints anything, and every open asserts it. Configuring the line with the
# stty command first therefore starts the run, and by the time the reader
# attaches the menu has already timed out and all four workloads are going.
# So the line is configured with termios on the fd we keep.
import os, select, sys, termios, time

import glob

# The board does not always come back on the same node - a watchdog reset
# re-enumerates and can move from ttyACM0 to ttyACM1 - so find it rather than
# assuming.
def wait_for_port(timeout=25):
    end = time.time() + timeout
    while time.time() < end:
        ports = sorted(glob.glob("/dev/ttyACM*"))
        if ports:
            time.sleep(0.8)  # settle after enumeration
            return ports[-1]
        time.sleep(0.2)
    return None

def main():
    key = (sys.argv[1] if len(sys.argv) > 1 else "4").encode()
    budget = float(sys.argv[2]) if len(sys.argv) > 2 else 180.0
    port = wait_for_port()
    if port is None:
        print("ERROR: no /dev/ttyACM* appeared")
        return 1
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
        iflag = 0
        oflag = 0
        lflag = 0
        cflag = termios.CS8 | termios.CREAD | termios.CLOCAL
        termios.tcsetattr(
            fd, termios.TCSANOW,
            [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
        out, sent, end = b"", False, time.time() + budget
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.5)
            if r:
                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    chunk = b""
                if chunk:
                    out += chunk
                    sys.stdout.write(chunk.decode("utf8", "replace"))
                    sys.stdout.flush()
            if not sent and b"wait 5 seconds" in out:
                os.write(fd, key)
                sent = True
            if b"done" in out:
                break
        if b"done" not in out:
            print("\n[timed out after %.0fs; %d bytes]" % (budget, len(out)))
        return 0
    finally:
        os.close(fd)

sys.exit(main())
