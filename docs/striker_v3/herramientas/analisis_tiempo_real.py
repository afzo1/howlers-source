#!/usr/bin/env python3
"""Analisis de planificabilidad de Striker v3 (seccion 3 de la Entrega 1).

Lee el conjunto de tareas de un archivo JSON y, para cada nucleo, calcula:
  - la utilizacion U = sum(Ci / Ti) y la cota de Liu y Layland n(2^(1/n) - 1);
  - el tiempo de respuesta en el peor caso de cada tarea con el analisis
    clasico de prioridad fija (Joseph y Pandya; Audsley et al.):

        R = C + B + sum_{j de mayor prioridad} ceil((R + Jj) / Tj) * Cj

    iterando hasta que R no cambie o supere el plazo D.

Convenciones:
  - Tiempos en microsegundos.
  - Las ISR y las tareas del sistema que interfieren se escriben como tareas de
    prioridad mayor (campo "interferencia": true): no se les calcula R.
  - Una tarea que se suspende esperando E/S (I2C, UART) se analiza de forma
    "suspension-oblivious": su tiempo de suspension se suma a C. Es pesimista
    pero seguro.
  - En la Entrega 2 se reemplazan los C presupuestados por los WCET medidos y
    se vuelve a correr el script.

Uso:  python3 analisis_tiempo_real.py tareas_v3.json [--variante S1|S2]
"""
import argparse
import json
import math
import sys


def wcet(t, variante):
    """C efectivo: numero fijo o dict por variante; mas la suspension por E/S."""
    c = t["C"]
    if isinstance(c, dict):
        c = c[variante]
    s = t.get("suspension", 0)
    if isinstance(s, dict):
        s = s[variante]
    return c + s


def bloqueo(t, variante):
    b = t.get("B", 0)
    return b[variante] if isinstance(b, dict) else b


def rta(task, higher, variante):
    c = wcet(task, variante)
    b = bloqueo(task, variante)
    d = task["D"]
    r = c + b
    for _ in range(1000):
        interf = sum(math.ceil((r + h.get("J", 0)) / h["T"]) * wcet(h, variante) for h in higher)
        r_new = c + b + interf
        if r_new == r:
            return r
        if r_new > d:
            return r_new  # no planificable: se devuelve el primer valor que excede D
        r = r_new
    return float("inf")


def planificable(tareas, variante, alfa):
    """True si todas las tareas propias cumplen R <= D con sus C de computo
    multiplicados por alfa (la suspension por E/S y las ISR no se escalan)."""
    escaladas = []
    for t in tareas:
        u = dict(t)
        if not t.get("interferencia"):
            c = t["C"][variante] if isinstance(t["C"], dict) else t["C"]
            u["C"] = c * alfa
        escaladas.append(u)
    for i, t in enumerate(escaladas):
        if t.get("interferencia"):
            continue
        if rta(t, escaladas[:i], variante) > t["D"]:
            return False
    return True


def factor_holgura(tareas, variante):
    """Mayor factor alfa (busqueda binaria) con el que el nucleo sigue siendo planificable."""
    if not planificable(tareas, variante, 1.0):
        return None
    lo, hi = 1.0, 2.0
    while planificable(tareas, variante, hi) and hi < 1000:
        lo, hi = hi, hi * 2
    for _ in range(40):
        mid = (lo + hi) / 2
        if planificable(tareas, variante, mid):
            lo = mid
        else:
            hi = mid
    return lo


def analizar(datos, variante):
    salida = []
    for nucleo in datos["nucleos"]:
        tareas = sorted(nucleo["tareas"], key=lambda t: -t["prioridad"])
        propias = [t for t in tareas if not t.get("interferencia")]
        u_total = sum(wcet(t, variante) / t["T"] for t in tareas)
        u_cpu = sum((t["C"][variante] if isinstance(t["C"], dict) else t["C"]) / t["T"] for t in tareas)
        n = len(tareas)
        cota = n * (2 ** (1 / n) - 1)
        salida.append(f"\n### {nucleo['nombre']} (variante {variante})\n")
        salida.append("| Prio | Tarea | T (µs) | D (µs) | C (µs) | B (µs) | J (µs) | U | R (µs) | ¿R ≤ D? |")
        salida.append("|---:|---|---:|---:|---:|---:|---:|---:|---:|:---:|")
        todas_ok = True
        for i, t in enumerate(tareas):
            mayores = tareas[:i]
            c = wcet(t, variante)
            u = c / t["T"]
            if t.get("interferencia"):
                salida.append(f"| {t['prioridad']} | {t['nombre']} | {t['T']} | — | {c:g} | — | {t.get('J', 0)} | {u:.3f} | — | — |")
                continue
            r = rta(t, mayores, variante)
            ok = r <= t["D"]
            todas_ok &= ok
            salida.append(
                f"| {t['prioridad']} | {t['nombre']} | {t['T']} | {t['D']} | {c:g} | {bloqueo(t, variante)} | "
                f"{t.get('J', 0)} | {u:.3f} | {r:g} | {'sí' if ok else '**NO**'} |"
            )
        salida.append("")
        salida.append(f"- Utilización con la E/S contada como cómputo: **U = {u_total:.3f}**; "
                      f"cota de Liu y Layland para n = {n}: {cota:.3f}.")
        salida.append(f"- Utilización real de CPU (sin la espera de E/S): **{u_cpu:.3f}**.")
        salida.append(f"- Resultado: {'todas las tareas cumplen R ≤ D' if todas_ok else '**hay tareas que no cumplen R ≤ D**'}.")
        alfa = factor_holgura(tareas, variante)
        if alfa is not None:
            salida.append(f"- Factor de holgura: el cómputo de las tareas propias puede multiplicarse por **{alfa:.2f}** "
                          f"antes de que alguna incumpla su plazo.")
    return "\n".join(salida)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("archivo")
    ap.add_argument("--variante", default=None, help="S1 o S2 (por defecto, ambas)")
    a = ap.parse_args()
    datos = json.load(open(a.archivo, encoding="utf-8"))
    variantes = [a.variante] if a.variante else datos.get("variantes", ["S1", "S2"])
    for v in variantes:
        print(analizar(datos, v))


if __name__ == "__main__":
    sys.exit(main())
