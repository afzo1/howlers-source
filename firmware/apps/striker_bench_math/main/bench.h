/*
 * Striker v3 - benchmark de matematicas (Entrega 2, paquete P2).
 * Funciones comunes: medicion de ciclos, tabla de resultados y datos de prueba.
 */
#pragma once

#include <stdint.h>

/* Un caso ejecuta su calculo n veces seguidas. */
typedef void (*bench_caso_t)(int n);

/* Ciclos de CPU por llamada: 100 llamadas de calentamiento y despues 5
 * repeticiones de 1000 llamadas. Devuelve la mediana de las 5. */
float bench_medir(bench_caso_t caso);

/* Tabla: un grupo por cada tipo de calculo. El speedup de cada fila se
 * calcula frente a ciclos_ref (la referencia del grupo). */
void bench_grupo(const char *titulo);
void bench_fila(const char *caso, const char *variante, float ciclos, float ciclos_ref);
void bench_nota(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Generador pseudoaleatorio con semilla fija: los mismos datos en cada corrida. */
uint32_t bench_azar(void);
float bench_azar_f(float min, float max);

/* Grupos de casos (cada uno imprime su parte de la tabla). */
void casos_pid(void);
void casos_matrices(void);
void casos_ekf(void);

/* Destinos volatiles: evitan que el compilador borre resultados no usados. */
extern volatile float bench_sumidero_f;
extern volatile double bench_sumidero_d;
extern volatile int32_t bench_sumidero_i;
