# Utility script to generate LAMMPS style lists for tools
# Written by Richard Berger (2016)
import os
import json
import tempfile
import subprocess

def generate_style_expansion(style_name):
    code = """
#define {STYLE}_CLASS
#define {Style}Style(key,Class) \\
#key
#include "style_{style}.h"
#undef {Style}Style
#undef {STYLE}_CLASS"""

    defs = {
        'style' : style_name.lower(),
        'Style' : style_name.title(),
        'STYLE' : style_name.upper(),
    }
    return code.format_map(defs)

def get_styles(style_name):
    f = tempfile.NamedTemporaryFile(mode="w+", delete=False, suffix=".cpp")
    f.write(generate_style_expansion(style_name))
    f.close()

    output = subprocess.check_output(["g++ -E -I. %s" % f.name],shell=True).decode()

    styles = []

    for line in output.splitlines():
        if not line or line.startswith("#"):
            continue
        styles.append(line.strip('"'))

    os.unlink(f.name)
    return sorted(styles)

with open('pair_styles.json', 'w') as f:
    json.dump(get_styles("pair"), f)

with open('fix_styles.json', 'w') as f:
    json.dump(get_styles("fix"), f)

with open('dump_styles.json', 'w') as f:
    json.dump(get_styles("dump"), f)

with open('compute_styles.json', 'w') as f:
    json.dump(get_styles("compute"), f)
