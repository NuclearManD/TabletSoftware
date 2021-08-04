
import sys, select


class StdIOStream:

    def write(self, data):
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()

    def available(self):
        return select.select([sys.stdin,],[],[],0.0)[0]

    def read(self):
        return sys.stdin.buffer.read(1)
