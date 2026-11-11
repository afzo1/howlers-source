#!/usr/bin/env python3
"""Verifica los enlaces de un documento Markdown de esta carpeta.

Revisa dos tipos de enlace:
  1. Relativos (../../firmware/...): que el archivo o la carpeta exista hoy.
  2. Permanentes a este repositorio en un commit fijo
     (https://github.com/<dueno>/<repo>/blob/<commit>/<ruta>#Lx-Ly): que la ruta
     exista en ese commit, leyendola con git.
En ambos casos, si el enlace trae lineas (#Lx o #Lx-Ly), comprueba que esten
dentro del archivo e imprime las primeras para revisarlas a mano. Si trae el
ancla de un titulo de Markdown (#mi-titulo), comprueba que el titulo exista,
con la misma regla que usa GitHub para formar las anclas.

Uso:  python3 verificar_enlaces.py <documento.md> [ruta_del_repo]
      (ruta_del_repo por defecto: dos carpetas arriba de docs/striker_v3)
"""
import os
import re
import subprocess
import sys

if len(sys.argv) < 2:
    sys.exit(__doc__)

md_path = os.path.abspath(sys.argv[1])
base = os.path.dirname(md_path)
repo = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.normpath(os.path.join(base, "..", ".."))
text = open(md_path, encoding="utf-8").read()
links = re.findall(r"\[([^\]]+)\]\(([^)\s]+)\)", text)
permalink = re.compile(r"https://github\.com/[^/]+/[^/]+/blob/([0-9a-f]{7,40})/([^#]+)(?:#(.*))?$")

problems = 0
seen = set()


def heading_slugs(lines):
    """Anclas que GitHub genera para los titulos de un Markdown."""
    slugs, seen_count, in_code = set(), {}, False
    for line in lines:
        if line.lstrip().startswith("```"):
            in_code = not in_code
            continue
        m = None if in_code else re.match(r"#{1,6}\s+(.*?)\s*#*\s*$", line)
        if not m:
            continue
        text = re.sub(r"`", "", m.group(1)).lower()
        slug = re.sub(r"[^\w\- ]", "", text).replace(" ", "-")
        n = seen_count.get(slug, 0)
        seen_count[slug] = n + 1
        slugs.add(slug if n == 0 else f"{slug}-{n}")
    return slugs


def git_lines(sha, path):
    r = subprocess.run(["git", "-C", repo, "show", f"{sha}:{path}"], capture_output=True)
    if r.returncode != 0:
        return None
    return r.stdout.decode("utf-8", errors="replace").splitlines()


def check_anchor(label, target, anchor, lines, shown):
    global problems
    if not anchor:
        return
    m = re.fullmatch(r"L(\d+)(?:-L(\d+))?", anchor)
    if not m:
        if shown.endswith(".md") and anchor in heading_slugs(lines):
            return
        print(f"ANCLA_INVALIDA {target}")
        problems += 1
        return
    a = int(m.group(1))
    b = int(m.group(2) or a)
    if a < 1 or a > b or b > len(lines):
        print(f"FUERA_DE_RANGO {target} (el archivo tiene {len(lines)} lineas)")
        problems += 1
        return
    print(f"--- [{label}] {shown}:{a}-{b}")
    for i in range(a, min(b, a + 3) + 1):
        print(f"    {i}: {lines[i - 1].strip()[:150]}")
    if b > a + 3:
        print(f"    ... ({b - a - 3} lineas mas)")


own_lines = text.splitlines()
for label, target in links:
    if (label, target) in seen:
        continue
    seen.add((label, target))
    if target.startswith("#"):
        if target[1:] not in heading_slugs(own_lines):
            print(f"ANCLA_INVALIDA {target} (en este mismo documento)")
            problems += 1
        continue
    m = permalink.match(target)
    if m:
        sha, path, anchor = m.group(1), m.group(2), m.group(3)
        lines = git_lines(sha, path)
        if lines is None:
            print(f"NO_EXISTE_EN_{sha[:7]} {path}")
            problems += 1
            continue
        check_anchor(label, target, anchor, lines, f"{sha[:7]}:{path}")
        continue
    if target.startswith(("http://", "https://", "mailto:")):
        continue  # enlaces externos: no se revisan
    path, _, anchor = target.partition("#")
    full = os.path.normpath(os.path.join(base, path))
    if not os.path.exists(full):
        print(f"NO_EXISTE {target}  (texto: {label})")
        problems += 1
        continue
    if anchor and os.path.isfile(full):
        lines = open(full, encoding="utf-8", errors="replace").read().splitlines()
        check_anchor(label, target, anchor, lines, os.path.relpath(full, repo))

print(f"\nEnlaces revisados: {len(seen)}  problemas: {problems}")
sys.exit(1 if problems else 0)
