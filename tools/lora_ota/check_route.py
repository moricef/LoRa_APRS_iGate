#!/usr/bin/env python3
"""Check mandatory relay use and profile negotiation before transferring an image."""
import argparse
import base64
import json
from pathlib import Path
import shlex
import subprocess
import time

from send_integrated import Client, HELLO, ABORT, NETWORK_STATUS, printable
from route_transport import RouteTransport


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('bridge', 'key-file', 'route-key-file', 'counter-file', 'log',
                 'relay-ssh', 'relay-directory'):
        ap.add_argument('--' + name, required=True)
    args = ap.parse_args()
    key = base64.urlsafe_b64decode(Path(args.key_file).read_text().strip() + '=')
    route_key = base64.urlsafe_b64decode(Path(args.route_key_file).read_text().strip() + '=')

    def admin(action):
        command = ('cd ' + shlex.quote(args.relay_directory) +
                   ' && python3 relay_admin.py ' + action +
                   ' --config F4MLV-10-before.json')
        result = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o',
                                 'ConnectTimeout=5', '-o', 'HostKeyAlias=thinkpad',
                                 args.relay_ssh, command], check=True,
                                capture_output=True, text=True, timeout=25)
        return json.loads(result.stdout) if action == 'stats' else result.stdout.strip()

    with Path(args.log).open('w') as log:
        def event(kind, **fields):
            row = dict(time=time.time(), event=kind, **fields)
            text = json.dumps(row)
            log.write(text + '\n')
            log.flush()
            print(text, flush=True)

        before = admin('stats')
        assert before['role'] == 1 and not before['active'], before
        c = Client(args.bridge, key, b'F4MLV-2', b'F4MLV-15',
                   Path(args.counter_file), event)
        paused = False
        try:
            c.normal_timeout = 35
            c.enable_profiles()
            counter = max(int(time.time()), int(c.counter_file.read_text()) + 1)

            def rejected(kind):
                try:
                    c.request(HELLO, counter, attempts=1, timeout=12)
                except TimeoutError:
                    event(kind, observation_seconds=12)
                else:
                    raise AssertionError('Target answered without the required relay')

            rejected('raw_direct_hello_no_reply')
            c.route = RouteTransport(route_key)
            c.chunk = 175
            admin('pause')
            paused = True
            assert admin('stats')['paused']
            rejected('paused_relay_hello_no_reply')
            admin('resume')
            paused = False
            initial = c.hello()
            assert not initial['pending'], initial
            network = c.request(NETWORK_STATUS)
            assert network['wifi_mode'] == 0 and not network['connected'] and not network['active_interface']
            event('routed_hello_target_offline', reply=printable(initial), network=printable(network))
            c.enter_fast(30)
            c.return_normal()
            c.request(ABORT)
            after = admin('stats')
            assert after['requests_forwarded'] > before['requests_forwarded']
            assert after['responses_forwarded'] > before['responses_forwarded']
            assert not after['active'] and after['profile'] == 0, after
            event('route_preflight_complete', relay_before=before, relay_after=after)
        finally:
            if paused:
                admin('resume')
            c.close()


if __name__ == '__main__':
    main()
