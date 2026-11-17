"""Minimal, non-UI Codex app-server probe for AI Workspace validation.

The probe reports protocol timings and event names only. It never prints
prompts, deltas, tokens, credentials, or server output verbatim.
"""

import argparse
import json
import queue
import subprocess
import sys
import threading
import time


def _reader(stream, channel, events, started):
    for line in stream:
        if channel == "stdout":
            try:
                events.put((channel, json.loads(line), time.monotonic() - started))
            except (TypeError, ValueError):
                continue
        else:
            events.put((channel, None, time.monotonic() - started))


def _wait_response(events, request_id, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            channel, message, elapsed = events.get(timeout=0.25)
        except queue.Empty:
            continue
        if channel == "stdout" and message.get("id") == request_id:
            return message, elapsed
    return None, None


def _send(process, next_id, method, params=None):
    request_id = next_id
    process.stdin.write(json.dumps({"id": request_id, "method": method, "params": params or {}}) + "\n")
    process.stdin.flush()
    return request_id, request_id + 1, time.monotonic()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codex", default="codex")
    parser.add_argument("--init-timeout", type=float, default=20.0)
    parser.add_argument("--first-event-timeout", type=float, default=20.0)
    parser.add_argument("--idle-timeout", type=float, default=30.0)
    parser.add_argument("--total-timeout", type=float, default=90.0)
    parser.add_argument("--interrupt-after-delta", action="store_true")
    args = parser.parse_args()

    started = time.monotonic()
    events = queue.Queue()
    process = subprocess.Popen(
        [args.codex, "app-server", "--listen", "stdio://"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    threading.Thread(target=_reader, args=(process.stdout, "stdout", events, started), daemon=True).start()
    threading.Thread(target=_reader, args=(process.stderr, "stderr", events, started), daemon=True).start()
    next_id = 1

    try:
        initialize_id, next_id, _ = _send(
            process,
            next_id,
            "initialize",
            {"clientInfo": {"name": "godot_ai_workspace_probe", "title": "Godot AI Workspace Probe", "version": "0.1.0"}},
        )
        initialize, initialize_elapsed = _wait_response(events, initialize_id, args.init_timeout)
        print(f"initialize_ms={initialize_elapsed * 1000:.0f}" if initialize_elapsed else "initialize=timeout")
        if not initialize or "error" in initialize:
            return 2

        process.stdin.write(json.dumps({"method": "initialized"}) + "\n")
        process.stdin.flush()
        thread_id_request, next_id, _ = _send(process, next_id, "thread/start")
        thread_response, thread_elapsed = _wait_response(events, thread_id_request, args.init_timeout)
        print(f"thread_start_ms={thread_elapsed * 1000:.0f}" if thread_elapsed else "thread_start=timeout")
        thread = ((thread_response or {}).get("result") or {}).get("thread") or {}
        thread_id = thread.get("id", "")
        if not thread_id:
            return 3

        turn_id, next_id, turn_sent = _send(
            process,
            next_id,
            "turn/start",
            {"threadId": thread_id, "input": [{"type": "text", "text": "Responda apenas: OK."}]},
        )
        first_event = None
        first_delta = None
        terminal = None
        turn_response = None
        interrupt_request = None
        interrupt_response = None
        active_turn_id = ""
        last_activity = time.monotonic()
        total_deadline = turn_sent + args.total_timeout
        first_event_deadline = turn_sent + args.first_event_timeout
        while time.monotonic() < total_deadline:
            try:
                channel, message, elapsed = events.get(timeout=0.25)
            except queue.Empty:
                now = time.monotonic()
                if first_event is None and now >= first_event_deadline:
                    print("first_event=timeout")
                    break
                if first_event is not None and now - last_activity >= args.idle_timeout:
                    print("idle_timeout=true")
                    break
                continue
            if channel != "stdout":
                continue
            last_activity = time.monotonic()
            method = message.get("method")
            if first_event is None:
                first_event = (method or "response", elapsed)
                print(f"first_event={first_event[0]}_ms={elapsed * 1000:.0f}")
            if method == "turn/started":
                active_turn_id = ((message.get("params") or {}).get("turn") or {}).get("id", active_turn_id)
            if message.get("id") == turn_id:
                active_turn_id = (((message.get("result") or {}).get("turn") or {}).get("id", active_turn_id))
            if message.get("id") == turn_id:
                turn_response = message
            if method == "item/agentMessage/delta" and first_delta is None:
                first_delta = elapsed
                print(f"first_delta_ms={elapsed * 1000:.0f}")
                if args.interrupt_after_delta:
                    interrupt_request, next_id, _ = _send(process, next_id, "turn/interrupt", {"threadId": thread_id, "turnId": active_turn_id})
                    print("interrupt_sent=true")
            if interrupt_request and message.get("id") == interrupt_request:
                interrupt_response = message
            if method in ("turn/completed", "turn/failed"):
                turn = (message.get("params") or {}).get("turn") or {}
                terminal = (method, turn.get("status", ""), elapsed)
                print(f"terminal={method} status={terminal[1]} elapsed_ms={elapsed * 1000:.0f}")
                break
            if method == "error":
                print("terminal=error")
                break

        if turn_response and "error" in turn_response:
            print("turn_response=error")
        else:
            print("turn_response=ok" if turn_response else "turn_response=timeout")
        if terminal:
            resume_request, next_id, _ = _send(process, next_id, "thread/resume", {"threadId": thread_id})
            resume_response, resume_elapsed = _wait_response(events, resume_request, args.init_timeout)
            print(f"thread_resume_ms={resume_elapsed * 1000:.0f}" if resume_elapsed else "thread_resume=timeout")
        if args.interrupt_after_delta:
            interrupted = terminal and terminal[1] == "interrupted"
            print("interrupt_response=ok" if interrupt_response else "interrupt_response=timeout")
            print("result=ok" if first_delta and interrupted else "result=inconclusive")
            return 0 if first_delta and interrupted else 4
        print("result=ok" if first_delta and terminal and terminal[0] == "turn/completed" else "result=inconclusive")
        return 0 if first_delta and terminal and terminal[0] == "turn/completed" else 4
    finally:
        try:
            process.terminate()
            process.wait(timeout=5)
        except (OSError, subprocess.TimeoutExpired):
            process.kill()


if __name__ == "__main__":
    sys.exit(main())
