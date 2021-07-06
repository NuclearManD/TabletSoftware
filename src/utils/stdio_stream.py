
import sys


class StdIOStream:

    def write(self, data):
        sys.stdout.write(data)

    def available(self):
        return select.select([sys.stdin,],[],[],0.0)[0]

    def read(self):
        return sys.stdin.read()
