import sys
import re
import numpy as np


def parse_trace(trace):
    # Open the trace file
    with open(trace, "r", encoding="utf-8") as f:
        # Read the lines of the trace file
        regex = re.compile(r"\[(\d+)\] - update_simulation: delta=(\d+)")
        regex_lvtick = re.compile(r"\[(\d+)\] - lv_tick_task")
        data = f.read()
        matches = regex.findall(data)
        t, s = zip(*matches)

        timestamps = np.array(t, dtype=int)
        deltas = np.array(s, dtype=int)

        # Calculate the average delta
        avg_delta = np.mean(deltas)
        print(f"Average delta: {avg_delta/1000} ms")
        # Calculate the standard deviation of the delta
        std_dev = np.std(deltas)
        print(f"Standard deviation: {std_dev/1000} ms")
        # Calculate the median of the delta
        median = np.median(deltas)
        print(f"Median: {median/1000} ms")

        diff = np.diff(timestamps)
        print(f"Average time between events: {np.mean(diff)/1000} ms")
        print(f"Standard deviation of time between events: {np.std(diff)/1000} ms")
        print(f"Median time between events: {np.median(diff)/1000} ms")
        print(f"Min time between events: {np.min(diff)/1000} ms")
        print(f"Max time between events: {np.max(diff)/1000} ms")

        matches = regex_lvtick.findall(data)
        ticks = np.array(matches, dtype=int)
        diff = np.diff(ticks)
        print(f"Average time between ticks: {np.mean(diff)/1000} ms")


if __name__ == "__main__":
    # Parse the trace file

    parse_trace(sys.argv[1])
