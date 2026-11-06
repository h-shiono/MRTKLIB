#!/usr/bin/env python3
"""Check that a restarted ``mrtk relay`` rebinds its tcpsvr port (#367).

Starts ``mrtk relay`` with a ``tcpsvr://`` output, connects a TCP client and
keeps it open, stops the relay (SIGTERM) and starts it again at once. The old
server socket is then in FIN-WAIT-2; without SO_REUSEADDR the second bind
fails with EADDRINUSE and the relay exits with "stream server start error".
The second relay must still be running after a few seconds and accept a new
client.

The relay input is a tcpsvr port that never receives data, so the relay idles
until it is stopped.

Usage:
    check_tcpsvr_rebind.py MRTK
"""

import signal
import socket
import subprocess
import sys
import time

START_WAIT = 2.0  # seconds for a relay to bind its ports
ALIVE_WAIT = 3.0  # the second relay must survive this long


def free_ports(n):
    """n distinct free ports: all sockets stay bound until every port is picked."""
    socks = [socket.socket() for _ in range(n)]
    try:
        for s in socks:
            s.bind(("127.0.0.1", 0))
        return [s.getsockname()[1] for s in socks]
    finally:
        for s in socks:
            s.close()


def start_relay(mrtk, port_in, port_out):
    cmd = [mrtk, "relay", "-in", f"tcpsvr://:{port_in}", "-out", f"tcpsvr://:{port_out}"]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)


def connect(port, timeout=START_WAIT):
    t_end = time.time() + timeout
    while True:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1.0)
        except OSError:
            if time.time() > t_end:
                raise
            time.sleep(0.1)


def stop(proc):
    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    return proc.stderr.read() if proc.stderr else ""


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    mrtk = sys.argv[1]
    port_in, port_out = free_ports(2)

    first = start_relay(mrtk, port_in, port_out)
    client = None
    try:
        client = connect(port_out)
        time.sleep(0.5)  # let the relay accept the client
        stop(first)  # the client stays open: server side goes to FIN-WAIT-2

        second = start_relay(mrtk, port_in, port_out)
        time.sleep(ALIVE_WAIT)
        alive = second.poll() is None
        accepted = False
        if alive:
            try:
                connect(port_out).close()
                accepted = True
            except OSError:
                pass
        err = stop(second)
    finally:
        if client:
            client.close()
        stop(first)

    if not alive or not accepted:
        print(f"FAIL: restarted relay alive={alive} accepted={accepted}", file=sys.stderr)
        print(err, file=sys.stderr)
        return 1
    print(f"ok: relay rebound tcpsvr port {port_out} with a client still connected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
