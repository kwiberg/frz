# Copyright 2021 Karl Wiberg
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import subprocess
import sys

license_text = rb"""
Copyright (\d{4} .*)

Licensed under the Apache License, Version 2[.]0 [(]the "License"[)];
you may not use this file except in compliance with the License[.]
You may obtain a copy of the License at

http://www[.]apache[.]org/licenses/LICENSE-2[.]0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied[.]
See the License for the specific language governing permissions and
limitations under the License[.]
""".strip()

def build_license_re():
    license_re = rb"(?:^/[*]\n)?"
    for line in license_text.splitlines():
        license_re += rb"^[# ]*%s\n" % line.strip()
    license_re += rb"(?:^[*]/\n)?^$"
    return re.compile(license_re, re.MULTILINE)
license_re = build_license_re()

def check_files():
    p = subprocess.run(["git", "ls-files", "-z"], capture_output=True)
    if p.stderr:
        print(p.stderr.decode("utf-8"))
        sys.exit(1)
    if p.returncode != 0:
        print("Return code %d" % p.returncode)
        sys.exit(1)
    files = sorted(p.stdout.split(b"\0"))
    copyright_owners = set()
    for fn in files:
        if re.match(b".*[.](cc|hh|py)$", fn):
            check_license(fn, copyright_owners)
    print("Copyright owners:")
    for co in sorted(copyright_owners):
        print("  %s" % co.decode("utf-8"))

def check_license(fn, copyright_owners):
    with open(fn, "rb") as f:
        contents = f.read()
    m = license_re.match(contents)
    if m:
        copyright_owners.add(m.group(1))
    else:
        print("*** No license text: %s" % fn.decode("utf-8"))

if __name__ == "__main__":
    check_files()
