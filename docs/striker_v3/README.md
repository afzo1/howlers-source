# Striker v3 — Entrega 1: plan de ingeniería sustentado

Documentación de diseño del firmware unificado `firmware/apps/striker_v3` (ESP32-S3), según la *Guía de Proyecto para Estudiantes: Diseño e Implementación del Nuevo Firmware Unificado Striker v3*.

Para empezar: [00_contexto_proyecto.md](00_contexto_proyecto.md) resume en una página qué se pide, dónde está cada cosa, qué se ha decidido y qué falta.

| Sección de la Entrega 1 | Documento | Estado |
|---|---|---|
| 1. Diagnóstico de referencia (Striker 1 y Striker 2) | [01_diagnostico_striker1_striker2.md](01_diagnostico_striker1_striker2.md) | Hecho (actualizado el 11-11-2026) |
| 1 (anexo). Correcciones mínimas para compilar las apps de referencia | [01-1_correcciones_compilacion.md](01-1_correcciones_compilacion.md) | Hecho |
| 1b. Diagnóstico de `bench_pid_autotuning` y de la suite de estimación | — | Pendiente |
| 2. Arquitectura por capas y diagrama de clases C/C++ | — | Pendiente |
| 3. Diseño FreeRTOS y tabla de tiempo real | — | Pendiente |
| 4. Mapa de memoria ESP32-S3 y diseño ULP | — | Pendiente |
| 5. Estrategia matemática y protocolo Web/UDP | — | Pendiente |

**Pruebas del robot heredado:** [guia_pruebas_robot.md](guia_pruebas_robot.md), paso a paso y desde cero, con el firmware de pruebas [`firmware/apps/striker_hw_test`](../../firmware/apps/striker_hw_test/README.md).

**Herramientas de apoyo** ([`herramientas/`](herramientas/)): instalación de ESP-IDF en Linux, compilación de todas las apps, análisis de operaciones `double` y verificación de enlaces.
