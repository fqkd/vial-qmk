"""Expose a bounded compiler diagnostic through the public check annotations."""
import pathlib
import re
import sys

text = pathlib.Path(sys.argv[1]).read_text(errors="replace")
text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text)
lines = text.splitlines()
errors = [line for line in lines if re.search(r"error:|Error |undefined reference|fatal|No rule|Assertion", line, re.I)]
message = "\n".join(errors or lines[-35:])[-12000:]
message = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
print("::error::" + message)
