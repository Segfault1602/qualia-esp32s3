import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
import warnings

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from parse_trace import parse_trace


class TraceTests(unittest.TestCase):
    def parse(self, contents):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.txt"
            trace.write_text(contents, encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output), warnings.catch_warnings():
                warnings.simplefilter("error", RuntimeWarning)
                parse_trace(trace)
            return output.getvalue()

    def test_simulation_records(self):
        output = self.parse(
            "Trace Start\n[1000000] - update_simulation: delta=100\n"
            "[1016666] - update_simulation: delta=200\n"
            "[1033332] - update_simulation: delta=300\n"
        )
        self.assertIn("Average delta: 0.2 ms", output)
        self.assertIn("Average time between events: 16.666 ms", output)

    def test_insufficient_records_are_rejected(self):
        for contents in ("", "[1000] - update_simulation: delta=100\n"):
            with self.subTest(contents=contents), self.assertRaisesRegex(ValueError, "at least two"):
                self.parse(contents)


if __name__ == "__main__":
    unittest.main()
