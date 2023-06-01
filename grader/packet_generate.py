from __future__ import annotations
import sys
import json
import struct
from dataclasses import dataclass
from base64 import b64decode


UDP_PROTO = 17
TCP_PROTO = 6
UDP_HDR_LEN = 8
TCP_HDR_LEN = 20
IP_HDR_LEN_MIN = 20
PSEUDO_HDR_LEN = 12
PSEUDO_HDR_FORMAT = '!IIBBH'
TCP_HDR_FORMAT = '!HHIIBBHHH'
UDP_HDR_FORMAT = '!HHHH'
IP_HDR_FORMAT = '!BBHHHBBHII'


def ip_to_num(ip: str) -> int:
    """
    Convert an IP address from string into Uint32

    :param ip: IP address in string
    :return: an uint32 representing the IP address (parsed from big endian).
    """
    vals = [int(x) for x in ip.split('.')]
    return (vals[0] << 24) | (vals[1] << 16) | (vals[2] << 8) | (vals[3])


class Checksum:
    """
    The class to compute IP/TCP/UDP checksum
    """
    val: int = 0

    def __init__(self):
        self.val = 0

    def add(self, buf: memoryview):
        """
        Add another buffer covered by the checksum

        :param buf: the buffer
        """
        # Treat the buffer as uint16 array and sum the values
        for i in range(0, len(buf) - 1, 2):
            self.val += (buf[i] << 8) + (buf[i + 1])
        # The last byte
        if len(buf) % 2 == 1:
            self.val += (buf[-1] << 8)

    def finish(self) -> int:
        """
        Return the result.

        :return: the checksum number. Irrelevant of the endian.
        """
        # Add the carries back. The loop runs at most twice.
        while self.val > 0xFFFF:
            self.val = (self.val >> 16) + (self.val & 0xFFFF)
        # The checksum result is the sum's one's complement.
        return ~self.val & 0xFFFF


@dataclass(unsafe_hash=True)
class Packet:
    src_ip: str
    src_port: int
    dst_ip: str
    dst_port: int
    proto: str
    payload: str
    ttl: int = 64
    tcp_seq: int = 0
    tcp_ack: int = 0
    tcp_flag: int = 0
    tcp_rwnd: int = 2048
    ip_options_b64: str = ''
    ip_checksum: int = -1
    trans_checksum: int = -1

    def serialize(self) -> bytes:
        """
        Encode the IP packet into bytes.

        :return: encoded bytes.
        """
        src_ip_num = ip_to_num(self.src_ip)
        dst_ip_num = ip_to_num(self.dst_ip)
        payload_enc = self.payload.encode('utf-8')
        if self.proto.lower() == 'tcp':
            proto = TCP_PROTO
            trans_length = len(self.payload) + TCP_HDR_LEN
        else:
            proto = UDP_PROTO
            trans_length = len(self.payload) + UDP_HDR_LEN
        # Pseudo IP header
        pseudo_hdr = bytearray(PSEUDO_HDR_LEN)
        struct.pack_into(
            PSEUDO_HDR_FORMAT, pseudo_hdr, 0,
            ip_to_num(self.src_ip),
            ip_to_num(self.dst_ip),
            0, proto, trans_length,
        )
        chksum = Checksum()
        chksum.add(memoryview(pseudo_hdr))
        chksum.add(memoryview(payload_enc))
        # Transport header
        if self.proto.lower() == 'tcp':
            tcp_hdr = bytearray(TCP_HDR_LEN)
            struct.pack_into(
                TCP_HDR_FORMAT, tcp_hdr, 0,
                self.src_port, self.dst_port,
                self.tcp_seq,
                self.tcp_ack,
                TCP_HDR_LEN << 2, self.tcp_flag, self.tcp_rwnd,
                0, 0,
            )
            chksum.add(memoryview(tcp_hdr))
            if self.trans_checksum < 0:
                struct.pack_into('!H', tcp_hdr, 16, chksum.finish())
            else:
                struct.pack_into('!H', tcp_hdr, 16, self.trans_checksum)
            trans_hdr = tcp_hdr
        else:
            udp_hdr = bytearray(8)
            struct.pack_into(
                UDP_HDR_FORMAT, udp_hdr, 0,
                self.src_port, self.dst_port,
                trans_length, 0,
            )
            chksum.add(memoryview(udp_hdr))
            if self.trans_checksum < 0:
                struct.pack_into('!H', udp_hdr, 6, chksum.finish())
            else:
                struct.pack_into('!H', udp_hdr, 6, self.trans_checksum)
            trans_hdr = udp_hdr
        # IP header
        ip_opt = b64decode(self.ip_options_b64)
        if len(ip_opt) % 4 != 0:
            ip_opt += b'\x00' * (4 - len(ip_opt) % 4)
        ip_hdr_len = IP_HDR_LEN_MIN + len(ip_opt)
        tot_len = ip_hdr_len + trans_length
        ip_hdr = bytearray(ip_hdr_len)
        struct.pack_into(
            IP_HDR_FORMAT, ip_hdr, 0,
            0x40 + ip_hdr_len // 4, 0, tot_len,
            0, 0x4000,
            self.ttl, proto, 0,
            src_ip_num,
            dst_ip_num,
        )
        ip_hdr[IP_HDR_LEN_MIN:] = ip_opt
        ip_chksum = Checksum()
        ip_chksum.add(memoryview(ip_hdr))
        if self.ip_checksum < 0:
            struct.pack_into('!H', ip_hdr, 10, ip_chksum.finish())
        else:
            struct.pack_into('!H', ip_hdr, 10, self.ip_checksum)
        # Total data
        return bytes(ip_hdr) + bytes(trans_hdr) + payload_enc

    @classmethod
    def from_json(cls, obj):
        """
        Create an IP packet from JSON setting.

        :param obj: the parsed JSON action object.
        :return: the packet.
        """
        return cls(obj['src_ip'], obj['src_port'], obj['dst_ip'], obj['dst_port'],
                   obj['proto'], obj['payload'], obj.get('ttl', 64), obj.get('seq', 0),
                   obj.get('ack', 0), obj.get('flag', 0), obj.get('rwnd', 2048),
                   obj.get('ip_options_b64', ''), obj.get('ip_checksum', -1), obj.get('trans_checksum', -1))
    
    def print_hex(self):
        print_hex(self.serialize())


def print_hex(buf: bytes):
    """
    Print a buffer in a hexadecimal format similar to Wireshark.

    :param buf: the buffer to print
    """
    bstr = buf.hex(' ')
    bstr_list = [bstr[i:i + 24] for i in range(0, len(bstr), 24)]
    lines = [' '.join(bstr_list[j:j + 2]) for j in range(0, len(bstr_list), 2)]
    for line in lines:
        print(line)


def separate_ip_packet(buf: bytes) -> bytes | None:
    """
    Separate IP datagram boundaries.

    :param buf: the received buffer.
    :return: The first IP datagram in the buffer.
        None if the buffer does not contain a whole IP datagram.
    """
    if len(buf) < IP_HDR_LEN_MIN:
        return None
    # The header len does not matter here
    ip_hdr = struct.unpack_from(IP_HDR_FORMAT, buf, 0)
    tot_len = ip_hdr[2]
    if len(buf) >= tot_len:
        return buf[:tot_len]
    else:
        return None


def main():
    text = ''.join(sys.stdin.readlines())
    actions = json.loads(text)['actions']
    check_point_cnt = 0
    for act in actions:
        if act['kind'] == 'send' or act['kind'] == 'expect':
            kind = 'SEND' if act['kind'] == 'send' else 'RECV'
            print(f'================== {kind} @@ {act["port"]:02} ==================')
            Packet.from_json(act).print_hex()
            print(f'================== ========== ==================\n')
        elif act['kind'] == 'check':
            check_point_cnt += 1
            print(f'Check point {check_point_cnt}\n')


if __name__ == '__main__':
    main()
