"""Track a preparation's supplied reads independently of caller history."""


class Inputs:
    def __init__(self, sources):
        self.sources = sources
        self.consumed = set()

    @property
    def pin(self):
        return self.sources.pin

    def paths(self, prefix):
        return self.sources.paths(prefix)

    def read(self, name):
        result = self.sources.read(name)
        self.consumed.add(name)
        return result
