#!/usr/bin/env python3
"""Lista, por función, las llamadas a rutinas de punto flotante de 64 bits (double) emuladas
por software en Xtensa LX7 (libgcc __*df*) y a funciones matemáticas double de libm.

Uso: buscar_double.py <objdump> <obj1> [<obj2> ...]
Se basa en las relocalizaciones de `objdump -dr` (llamadas a símbolos externos)."""
import re
import subprocess
import sys
from collections import defaultdict

DOUBLE_HELPERS = {
    "__adddf3", "__subdf3", "__muldf3", "__divdf3", "__negdf2", "__extendsfdf2", "__truncdfsf2",
    "__fixdfsi", "__fixunsdfsi", "__floatsidf", "__floatunsidf", "__eqdf2", "__nedf2", "__ltdf2",
    "__ledf2", "__gtdf2", "__gedf2", "__unorddf2", "__fixdfdi", "__floatdidf",
}
LIBM_DOUBLE = {"sin", "cos", "tan", "atan2", "atan", "asin", "acos", "exp", "log", "pow", "sqrt", "fabs", "floor", "ceil", "fmod", "round"}


def main() -> None:
    objdump = sys.argv[1]
    results = defaultdict(lambda: defaultdict(int))
    for obj in sys.argv[2:]:
        out = subprocess.run([objdump, "-dr", obj], capture_output=True, text=True, check=False).stdout
        func = None
        for line in out.splitlines():
            m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
            if m:
                # Cada llamada genera una relocalización en .text y otra en el literal pool;
                # se cuentan solo las de .text (una por punto de llamada).
                name = m.group(1)
                func = None if name.startswith(".literal") else name
                continue
            m = re.search(r"R_XTENSA_\w+\s+([\w.]+)", line)
            if m and func:
                sym = m.group(1)
                if sym in DOUBLE_HELPERS or sym in LIBM_DOUBLE:
                    results[(obj.split("/")[-1].replace(".c.obj", ".c"), func)][sym] += 1
    for (src, func), syms in sorted(results.items()):
        detail = ", ".join(f"{s}x{n}" for s, n in sorted(syms.items()))
        print(f"{src}\t{func}\t{detail}")


if __name__ == "__main__":
    main()
