"""Compile the installed M5 startup method, never a hand-copied equivalent."""
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
start = source.index("  bool M5Unified::_begin_rtc_imu(")
brace = source.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
Path(sys.argv[2]).write_text("namespace m5 {\n" + source[start:end] + "\n}\n")
