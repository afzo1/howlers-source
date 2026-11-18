#!/usr/bin/env python3
"""Verifica los enlaces de un documento Markdown de esta carpeta.

Revisa tres tipos de enlace:
  1. Relativos (../../firmware/...): que el archivo o la carpeta exista hoy.
  2. Permanentes a un commit fijo
     (https://github.com/<dueno>/<repo>/blob/<commit>/<ruta>#Lx-Ly): que la ruta
     exista en ese commit, leyendola con git. Los de howlers-source se leen en
     este repositorio; los de otro repositorio, en una copia local indicada con
     la variable VERIFICAR_REPOS="dueno/repo=/ruta/copia;dueno2/repo2=/ruta2".
  3. A ESP-IDF en una version fija
     (https://github.com/espressif/esp-idf/blob/v5.5.1/<ruta>#Lx-Ly): se leen en
     la copia local de $IDF_PATH, que debe estar en esa misma version.
En todos los casos, si el enlace trae lineas (#Lx o #Lx-Ly), comprueba que esten
dentro del archivo e imprime las primeras para revisarlas a mano. Si trae el
ancla de un titulo de Markdown (#mi-titulo), comprueba que el titulo exista,
con la misma regla que usa GitHub para formar las anclas. Los enlaces de los
tipos 2 y 3 sin copia local se cuentan como "sin revisar", no como problemas.

Uso:  python3 verificar_enlaces.py <documento.md> [ruta_del_repo]
      (ruta_del_repo por defecto: dos carpetas arriba de docs/striker_v3)
"""
import os
import re
import subprocess
import sys
from urllib.parse import unquote

if len(sys.argv) < 2:
    sys.exit(__doc__)

md_path = os.path.abspath(sys.argv[1])
base = os.path.dirname(md_path)
repo = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.normpath(os.path.join(base, "..", ".."))
text = open(md_path, encoding="utf-8").read()
links = re.findall(r"\[([^\]]+)\]\(([^)\s]+)\)", text)
permalink = re.compile(r"https://github\.com/([^/]+/[^/]+)/blob/([0-9a-f]{7,40})/([^#]+)(?:#(.*))?$")
idf_link = re.compile(r"https://github\.com/espressif/esp-idf/blob/(v[0-9][^/]*)/([^#]+)(?:#(.*))?$")

# Copias locales de otros repositorios: VERIFICAR_REPOS="dueno/repo=/ruta;..."
clones = {}
for item in filter(None, os.environ.get("VERIFICAR_REPOS", "").split(";")):
    name, _, path = item.partition("=")
    clones[name.strip().lower()] = os.path.expanduser(path.strip())
idf_path = os.environ.get("IDF_PATH", "")

problems = 0
unchecked = 0
seen = set()
idf_tag_cache = {}


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


def git_lines(sha, path, where=None):
    r = subprocess.run(["git", "-C", where or repo, "show", f"{sha}:{path}"], capture_output=True)
    if r.returncode != 0:
        return None
    return r.stdout.decode("utf-8", errors="replace").splitlines()


def idf_version_ok(tag):
    """True si $IDF_PATH es una copia de ESP-IDF exactamente en la etiqueta pedida."""
    if tag not in idf_tag_cache:
        ok = False
        if idf_path and os.path.isdir(idf_path):
            r = subprocess.run(["git", "-C", idf_path, "describe", "--tags", "--exact-match"],
                               capture_output=True)
            ok = r.returncode == 0 and r.stdout.decode().strip() == tag
        idf_tag_cache[tag] = ok
    return idf_tag_cache[tag]


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
    m = idf_link.match(target)
    if m:
        tag, path, anchor = m.group(1), unquote(m.group(2)), m.group(3)
        if not idf_version_ok(tag):
            unchecked += 1
            continue
        full = os.path.join(idf_path, path)
        if not os.path.isfile(full):
            print(f"NO_EXISTE_EN_ESP-IDF_{tag} {path}")
            problems += 1
            continue
        lines = open(full, encoding="utf-8", errors="replace").read().splitlines()
        check_anchor(label, target, anchor, lines, f"esp-idf {tag}:{path}")
        continue
    m = permalink.match(target)
    if m:
        name, sha, path, anchor = m.group(1).lower(), m.group(2), unquote(m.group(3)), m.group(4)
        if name.endswith("/howlers-source"):
            where = repo
        elif name in clones:
            where = clones[name]
        else:
            unchecked += 1
            continue
        lines = git_lines(sha, path, where)
        if lines is None:
            print(f"NO_EXISTE_EN_{sha[:7]} {name}:{path}")
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

print(f"\nEnlaces revisados: {len(seen) - unchecked}  problemas: {problems}")
if unchecked:
    print(f"Sin revisar: {unchecked} (a ESP-IDF sin $IDF_PATH en esa version, o a otro "
          "repositorio sin copia en VERIFICAR_REPOS)")
sys.exit(1 if problems else 0)
