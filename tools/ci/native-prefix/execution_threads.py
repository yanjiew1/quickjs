"""Use every CPU available to the current hosted process."""
import os


def available_cpus():
    try:
        count = len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        count = os.cpu_count() or 1
    return max(1, count)


def test262_workers(argv):
    result = list(argv)
    position = result.index('-T')
    result[position + 1] = str(available_cpus())
    return result
