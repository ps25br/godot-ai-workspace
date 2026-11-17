#!/usr/bin/env python3
"""Small standard-library client for the local AI Workspace Agent Bridge."""

import argparse
import json
import socket
import sys


def request(args, message, timeout=5.0):
    payload = (json.dumps(message, separators=(",", ":")) + "\n").encode("utf-8")
    with socket.create_connection((args.host, args.port), timeout=timeout) as connection:
        connection.settimeout(timeout)
        connection.sendall(payload)
        data = bytearray()
        while b"\n" not in data:
            chunk = connection.recv(4096)
            if not chunk:
                break
            data.extend(chunk)
            if len(data) > 256 * 1024:
                raise RuntimeError("response exceeds the documented limit")
        if not data:
            raise RuntimeError("bridge closed the connection without a response")
        return json.loads(bytes(data).split(b"\n", 1)[0].decode("utf-8"))


def message(args, method, params=None, request_id="client-request"):
    return {
        "protocol": "ai-workspace/1",
        "id": request_id,
        "auth": args.secret,
        "method": method,
        "params": params or {},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=47831)
    parser.add_argument("--secret", required=True)
    subparsers = parser.add_subparsers(dest="operation", required=True)
    subparsers.add_parser("ping")
    subparsers.add_parser("context")
    command_parser = subparsers.add_parser("command")
    command_parser.add_argument("command_name")
    command_parser.add_argument("--args-json", default="{}")
    test_parser = subparsers.add_parser("self-test")
    test_parser.add_argument("--oversized-bytes", type=int, default=65 * 1024)

    args = parser.parse_args()
    if args.operation == "ping":
        request_message = message(args, "bridge.ping")
    elif args.operation == "context":
        request_message = message(args, "context.get")
    elif args.operation == "command":
        try:
            command_args = json.loads(args.args_json)
        except json.JSONDecodeError as error:
            parser.error(f"invalid --args-json: {error}")
        request_message = message(args, "command.execute", {"command": args.command_name, "args": command_args})
    else:
        checks = [
            ("ping", message(args, "bridge.ping", request_id="self-test-ping")),
            ("context", message(args, "context.get", request_id="self-test-context")),
            ("unknown_method", message(args, "bridge.unknown", request_id="self-test-unknown")),
            ("invalid_protocol", {**message(args, "bridge.ping", request_id="self-test-protocol"), "protocol": "ai-workspace/999"}),
            ("invalid_json", None),
        ]
        for name, check in checks:
            if check is None:
                raw = b"{not-json}\n"
                with socket.create_connection((args.host, args.port), timeout=5) as connection:
                    connection.settimeout(5)
                    connection.sendall(raw)
                    data = bytearray()
                    while b"\n" not in data:
                        chunk = connection.recv(4096)
                        if not chunk:
                            break
                        data.extend(chunk)
                    result = json.loads(bytes(data).split(b"\n", 1)[0].decode("utf-8"))
            else:
                result = request(args, check)
            print(json.dumps({name: result}, indent=2, sort_keys=True))
        oversized = {
            "protocol": "ai-workspace/1",
            "id": "self-test-oversized",
            "auth": args.secret,
            "method": "bridge.ping",
            "params": {"padding": "x" * args.oversized_bytes},
        }
        try:
            print(json.dumps({"oversized": request(args, oversized)}))
        except (ConnectionError, OSError, RuntimeError) as error:
            print(json.dumps({"oversized": "connection rejected: " + str(error)}))
        return 0

    print(json.dumps(request(args, request_message), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
