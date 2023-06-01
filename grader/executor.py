from __future__ import annotations
import os
import sys
import json
import asyncio as aio
from dataclasses import dataclass
from packet_generate import Packet, print_hex, separate_ip_packet


MAX_PACKET_SIZE = 65536
SERVER_PORT = 5152


@dataclass
class CheckErr:
    reason: str
    port_id: int = 0
    packet: bytes | None = None


class Connection:
    writer: aio.StreamWriter | None
    reader: aio.StreamReader | None
    received: list[bytes]
    expected: list[bytes]
    running: bool
    buf: bytes
    port_id: int = 0

    def __init__(self, port_id: int):
        self.writer = None
        self.reader = None
        self.received = []
        self.expected = []
        self.running = False
        self.buf = b''
        self.port_id = port_id

    async def start(self, port: int) -> CheckErr | None:
        try:
            self.reader, self.writer = await aio.open_connection('127.0.0.1', port)
            aio.create_task(self._run())
            return None
        except (TimeoutError, OSError):
            return CheckErr("Unable to connect to the server", port_id=self.port_id)

    def send(self, pkt: bytes) -> CheckErr | None:
        try:
            self.writer.write(pkt)
            return None
        except (OSError, RuntimeError):
            return CheckErr("Unable to send a packet to the server", port_id=self.port_id)

    def expect(self, pkt: bytes):
        self.expected.append(pkt)

    async def _run(self):
        self.running = True
        while True:
            recv_data = await self.reader.read(MAX_PACKET_SIZE)
            if not recv_data:
                break
            # Split packets
            self.buf += recv_data
            while True:
                pkt = separate_ip_packet(self.buf)
                if not pkt:
                    break
                self.buf = self.buf[len(pkt):]
                self.received.append(pkt)
        self.running = False
        self.writer.close()

    def check(self) -> CheckErr | None:
        if not self.running:
            return CheckErr("The server shutdown the connection before checkpoint", port_id=self.port_id)
        for p in self.received:
            try:
                idx = self.expected.index(p)
                self.expected = self.expected[:idx] + self.expected[idx+1:]
            except ValueError:
                return CheckErr("Unexpected packet", port_id=self.port_id, packet=p)
        if self.expected:
            p = self.expected[0]
            return CheckErr("Expected packet not received", port_id=self.port_id, packet=p)
        if self.buf:
            return CheckErr("Unexpected packet", port_id=self.port_id, packet=self.buf)
        self.expected = []
        self.received = []
        return None

    async def shutdown(self):
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()


class Executor:
    proc: aio.subprocess.Process | None
    running: bool
    setting_path: str
    server_path: str
    setting: dict[str, any]
    input_text: str
    conn_cnt: int
    conns: list[Connection]
    server_task: aio.Task[tuple[bytes, bytes]] | None

    async def exec_server(self, path: str, prog_input: bytes):
        self.running = True
        self.proc = await aio.create_subprocess_exec(
            path,
            stdin=aio.subprocess.PIPE,
            stdout=aio.subprocess.PIPE,
            stderr=aio.subprocess.PIPE,
        )

        async def wait_for_exit():
            pout, perr = await self.proc.communicate(prog_input)
            self.running = False
            return pout, perr

        self.server_task = aio.create_task(wait_for_exit())

    def parse_setting(self):
        """
        Parse the test case setting JSON file.
        """
        with open(self.setting_path) as f:
            self.setting = json.load(fp=f)
        input_str = self.setting['input']
        base_dir = os.path.dirname(self.setting_path)
        input_path = os.path.join(base_dir, input_str)
        with open(input_path) as f:
            self.input_text = f.read()

        # Check conn_cnt
        lines = self.input_text.splitlines()
        self.conn_cnt = 0
        while lines[self.conn_cnt+1]:
            self.conn_cnt += 1

    async def start(self, start_server: bool) -> CheckErr | None:
        """
        Start the server program and execute the test

        :param start_server: True if the test script starts the server as a new process.
            False if the test script connects to a running server.
        :return: the first error occurred in the test.
        """
        # Setup server
        if start_server:
            await self.exec_server(self.server_path, self.input_text.encode('utf-8'))
            await aio.sleep(0.5)
        # Setup connections
        self.conns = [Connection(i) for i in range(self.conn_cnt)]
        for conn in self.conns:
            err = await conn.start(SERVER_PORT)
            if err is not None:
                return err
            await aio.sleep(0.1)

        # Execute actions
        check_point_cnt = 0
        for act in self.setting['actions']:
            if act['kind'] == 'send':
                pkt = Packet.from_json(act)
                err = self.conns[act['port']].send(pkt.serialize())
                if err is not None:
                    return err
                aio.sleep(0.05)  # 50ms gap
            elif act['kind'] == 'expect':
                pkt = Packet.from_json(act)
                self.conns[act['port']].expect(pkt.serialize())
            elif act['kind'] == 'check':
                await aio.sleep(act['delay'])
                for conn in self.conns:
                    err = conn.check()
                    if err is not None:
                        return err
                check_point_cnt += 1
                print(f'Passed check point {check_point_cnt}')

    async def clean_up(self) -> tuple[bytes, bytes]:
        """
        Shutdown connections and kill the program started.

        :return: server's output in stdout and stderr.
        """
        for conn in reversed(self.conns):
            await conn.shutdown()
            await aio.sleep(0.05)
        if self.server_task:
            # Kill server if it is still running
            if self.proc and self.running:
                try:
                    self.proc.terminate()
                    await aio.sleep(0.1)
                    self.proc.kill()
                except ProcessLookupError:
                    pass
            return await self.server_task
        else:
            return b'', b''

    def __init__(self, setting_path: str, server_path: str):
        self.setting_path = setting_path
        self.server_path = server_path
        self.proc = None
        self.server_task = None


async def main():
    if len(sys.argv) < 3:
        print('Usage: python3 executor.py <path-to-server> <path-to-setting.json>')
        return

    executor = Executor(sys.argv[2], sys.argv[1])
    executor.parse_setting()
    err = await executor.start(True)  # Set this to False to manually start your server
    pout, perr = await executor.clean_up()
    if err is not None:
        print(err.reason, '@', err.port_id)
        if err.packet:
            print(print_hex(err.packet))
    else:
        print('OK')
    with open('stdout.txt', 'wb') as f:
        f.write(pout)
    with open('stderr.txt', 'wb') as f:
        f.write(perr)


if __name__ == '__main__':
    aio.run(main())
