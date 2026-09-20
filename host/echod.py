#!/usr/bin/env python3
"""Echo daemon -- the far end of the C64's modem line, for transport testing.

VICE connects a TCP socket to this and bridges it to the emulated user port.
Every byte the C64 sends is logged with its hex value, which is how we find
out what the machine is actually putting on the wire (PETSCII is not ASCII,
and cc65 re-encodes string literals at compile time).

Each received line is echoed back as "PONG <line>".
"""

import argparse
import socket
import sys
import threading


def serve_client(conn, addr, verbose):
    print("[echod] connected: %s" % (addr,), flush=True)
    buf = bytearray()
    try:
        while True:
            chunk = conn.recv(256)
            if not chunk:
                break
            if verbose:
                print("[echod] rx %s  %r"
                      % (" ".join("%02X" % b for b in chunk), bytes(chunk)),
                      flush=True)
            buf += chunk
            while True:
                for term in (b"\r", b"\n"):
                    if term in buf:
                        line, _, rest = buf.partition(term)
                        buf[:] = rest
                        reply = b"PONG " + bytes(line) + b"\r"
                        print("[echod] tx %s" % reply, flush=True)
                        conn.sendall(reply)
                        break
                else:
                    break
    except OSError as exc:
        print("[echod] %s" % exc, flush=True)
    finally:
        print("[echod] disconnected: %s" % (addr,), flush=True)
        conn.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=25232)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(4)
    print("[echod] listening on %s:%d" % (args.host, args.port), flush=True)

    try:
        while True:
            conn, addr = srv.accept()
            threading.Thread(target=serve_client,
                             args=(conn, addr, not args.quiet),
                             daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        srv.close()


if __name__ == "__main__":
    sys.exit(main())
