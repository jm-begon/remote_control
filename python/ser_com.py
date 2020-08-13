"""
TODO:
 [ ] Byte conversion. How is it working with Arduino ?
 [ ] Arduino part of the protocol
 [ ] More efficient communication than ascii numbering
"""
from functools import partial
from enum import Enum

import logging
import serial

class Protocol(Enum):
    UNKNOWN = 0
    NEC = 1
    SONY = 2
    NECX = 7

NUM_N_BYTES = 4




class Ack(object):
    def __init__(self, label, nac_info=None):
        self.label = label
        self.nac_info = nac_info

    def __bool__(self):
        return self.nac_info is None

    def __repr__(self):
        return "{}({})".format(self.__class__.__name__,
                               repr(self.label),
                               repr(self.nac_info))

    def __str__(self):
        if self:
            return "[ACK] {}".format(self.label)
        return "[NAC] {} -- Reason: {}".format(self.label, self.nac_info)



class Message(object):
    @property
    def label(self):
        return self.__class__.__name__
    def _write(self, channel, *bytes):
        channel.write(bytes)

    def _n2b(self, number, length=NUM_N_BYTES):
        bytes = number.to_bytes(length, "big")
        check = (sum(bytes) % 256).to_bytes(length, "big")
        return bytes, check

    def _read_check(self, channel, *bytes, label=None):
        ack = partial(Ack, label=self.label if label is None else label)

        for i, check_byte in enumerate(bytes):
            ans = channel.read(1)
            if len(ans) == 0:
                return ack(nac_info="Error with {}th byte. Timeout?".format(i))
            if ans != check_byte:
                return ack(nac_info="Error with {}th byte. Expecting {}, got {}"
                           "".format(i, check_byte, ans))
        return ack()


    def send_through(self, channel):
        yield Ack("Abstract message")


class Handshake(Message):
    def send_through(self, channel):
        # Send 'syn'
        # Recieve 'synack'
        # Send 'ack'

        self._write(channel, *b'syn')
        yield self._read_check(channel, *b'synack')
        self._write(channel, *b'ack')


class Command(Message):
    def __init__(self, code, protocol, size=None):
        self.code = code
        self.protocol = protocol
        self.size = size

    def send_through(self, channel):
        # Send protocol
        # Receive protocol
        b_protocol, check = self._n2b(self.protocol, length=1)
        self._write(channel, *b_protocol)
        p_ack = self._read_check(channel, *check,
                                 label="Communicating protocol")
        yield p_ack


        # Send size
        # Receive size
        b_size, check = self._n2b(self.size, length=1)
        self._write(channel, *b_size)
        s_ack =  self._read_check(channel, *check,
                                 label="Communicating protocol")
        yield s_ack

        # Send code
        bytes, check = self._n2b(self.code)
        self._write(channel, *bytes)
        c_ack = self._read_check(channel, *check,
                                 label="Giving command")
        yield c_ack

        # Confirming
        if p_ack and s_ack and c_ack:
            self._write(channel, *b'ok')
            yield self._read_check(channel, *b'sent')




class RemoteController(object):
    count = 0

    @classmethod
    def gen_name(cls):
        c = cls.count
        cls.count += 1
        return "RC{}".format(c)

    def __init__(self, port, baudrate=9600, timeout=1, name=None,
                 fail_fast=True):
        # self.con_factory = partial(serial.Serial, port=port, baudrate=baudrate,
        #                            timeout=timeout, writeTimeout=timeout)
        self.con_factory = partial(PseudoChannel, port=port, baudrate=baudrate,
                                    timeout=timeout, writeTimeout=timeout)
        self.connection = None
        name = self.__class__.gen_name() if name is None else name
        self.logger = logging.getLogger(name)
        self.fail_fast = fail_fast


    def __enter__(self):
        self.connection = self.con_factory()
        self.connection.open()
        self.send(Handshake(), fail_fast=True)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.connection is not None:
            self.connection.close()
        self.connection = None



    def send(self, message, fail_fast=False):
        fail_fast = self.fail_fast or fail_fast
        if not self.connection or not self.connection.isOpen():
            raise ConnectionError(repr(self.connection))
        for ack in message.send_through(self.connection):
            log_to = self.logger.debug if ack else self.logger.warning
            log_to(str(ack))
            if fail_fast and not ack:
                raise IOError(str(ack))


class PseudoChannel(object):
    def __init__(self, port, baudrate, timeout, writeTimeout):
        self.opened = False
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.writeTimeout = writeTimeout
        self.historic = []
        self.read_buffer = []
        self.logger = logging.getLogger("Pseudo channel")

    def __repr__(self):
        return "{}(port={}, baudrate={}, timeout={}, writeTimeout={}).{}" \
               "".format(self.__class__.__name__,
                         repr(self.port),
                         repr(self.baudrate),
                         repr(self.timeout),
                         repr(self.writeTimeout),
                         "open()" if self.opened else "close()")

    def open(self):
        self.opened = True

    def close(self):
        self.opened = False

    def isOpen(self):
        return self.opened

    def write(self, data):
        self.historic.append(data)
        print("[W]", data)
        self.logger.info("[W] {}".format(data))
        if data == b'syn':
            self.logger.info("[W] SYN")
            self.read_buffer.append(b'synack')
        elif data == b'ok':
            self.logger.info("[W] OK")
            self.read_buffer.append(b'sent')
        else:
            self.logger.info("[W] byte")
            num = int.from_bytes(data, "big")
            q = num
            check = 0
            while q >= 256:
                q, r = int(q / 256), q % 256
                check += r
            check += q
            self.read_buffer.append(check)

    def read(self, n):
        head, tail = self.read_buffer[:n], self.read_buffer[n:]
        self.read_buffer = tail
        self.logger.info("[R] {}".format(head))

        return head







if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)

    with RemoteController('COM1') as controller:
        controller.send(Command(3772793023, Protocol.NECX))
