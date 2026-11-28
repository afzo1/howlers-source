/*
 * Casos de multiplicacion de matrices en float: C simple frente a esp-dsp.
 *
 * Variantes (todas en la misma app, sin cambiar menuconfig):
 *   - C simple: tres bucles for, compilado con -O2.
 *   - dspm_mult_f32_ansi: esp-dsp escrita en C.
 *   - dspm_mult_f32_ae32: esp-dsp en ensamblador con la FPU, sin SIMD.
 *   - dspm_mult_f32_aes3: esp-dsp para el ESP32-S3, con instrucciones SIMD
 *     (operan sobre 4 floats a la vez). Solo usa SIMD si m, n y k son multiplos
 *     de 4 y las tres matrices estan alineadas a 16 bytes; si no, usa la ruta
 *     ae32 sin avisar.
 *   - Para 3x3 y 4x4, ademas, las versiones fijas dspm_mult_3x3x3_f32_ae32 y
 *     dspm_mult_4x4x4_f32_ae32.
 * Cada variante se compara con el resultado de C simple.
 */
#include <math.h>
#include <stdio.h>

#include "esp_dsp.h"

#include "bench.h"

#define N_MAX 16

static float s_a[N_MAX * N_MAX] __attribute__((aligned(16)));
static float s_b[N_MAX * N_MAX] __attribute__((aligned(16)));
static float s_c[N_MAX * N_MAX] __attribute__((aligned(16)));
static float s_ref[N_MAX * N_MAX];
static int s_n;

static __attribute__((noinline)) void mult_c(const float *a, const float *b, float *c, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float suma = 0.0f;
            for (int k = 0; k < n; k++) {
                suma += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = suma;
        }
    }
}

static void caso_c(int veces)
{
    for (int v = 0; v < veces; v++) {
        mult_c(s_a, s_b, s_c, s_n);
    }
}

static void caso_ansi(int veces)
{
    for (int v = 0; v < veces; v++) {
        dspm_mult_f32_ansi(s_a, s_b, s_c, s_n, s_n, s_n);
    }
}

static void caso_ae32(int veces)
{
    for (int v = 0; v < veces; v++) {
        dspm_mult_f32_ae32(s_a, s_b, s_c, s_n, s_n, s_n);
    }
}

static void caso_aes3(int veces)
{
    for (int v = 0; v < veces; v++) {
        dspm_mult_f32_aes3(s_a, s_b, s_c, s_n, s_n, s_n);
    }
}

static void caso_3x3_ae32(int veces)
{
    for (int v = 0; v < veces; v++) {
        dspm_mult_3x3x3_f32_ae32(s_a, s_b, s_c);
    }
}

static void caso_4x4_ae32(int veces)
{
    for (int v = 0; v < veces; v++) {
        dspm_mult_4x4x4_f32_ae32(s_a, s_b, s_c);
    }
}

/* Mide la variante, revisa que el resultado coincida con C simple y agrega la fila. */
static void medir_y_revisar(const char *variante, bench_caso_t caso, float ref)
{
    for (int i = 0; i < s_n * s_n; i++) {
        s_c[i] = 0.0f;
    }
    float ciclos = bench_medir(caso);
    float dif = 0.0f;
    for (int i = 0; i < s_n * s_n; i++) {
        dif = fmaxf(dif, fabsf(s_c[i] - s_ref[i]));
    }
    bench_fila("producto A x B", variante, ciclos, ref);
    if (dif > 1e-4f * (float)s_n) {
        bench_nota("ERROR: el resultado de %s difiere de C simple en %.2e", variante, (double)dif);
    }
}

void casos_matrices(void)
{
    static const int tamanos[] = {3, 4, 6, 8, 16};
    static const char *titulos[] = {
        "Matrices 3x3 (float)", "Matrices 4x4 (float)", "Matrices 6x6 (float)",
        "Matrices 8x8 (float)", "Matrices 16x16 (float)",
    };

    for (int t = 0; t < (int)(sizeof(tamanos) / sizeof(tamanos[0])); t++) {
        s_n = tamanos[t];
        for (int i = 0; i < s_n * s_n; i++) {
            s_a[i] = bench_azar_f(-1.0f, 1.0f);
            s_b[i] = bench_azar_f(-1.0f, 1.0f);
        }
        mult_c(s_a, s_b, s_ref, s_n);

        bench_grupo(titulos[t]);
        float ref = bench_medir(caso_c);
        bench_fila("producto A x B", "C simple (compilador)", ref, ref);
        medir_y_revisar("esp-dsp ansi (C)", caso_ansi, ref);
        medir_y_revisar("esp-dsp ae32 (FPU, sin SIMD)", caso_ae32, ref);
        medir_y_revisar("esp-dsp aes3 (SIMD del S3)", caso_aes3, ref);
        if (s_n == 3) {
            medir_y_revisar("esp-dsp 3x3x3 ae32 (fija)", caso_3x3_ae32, ref);
        } else if (s_n == 4) {
            medir_y_revisar("esp-dsp 4x4x4 ae32 (fija)", caso_4x4_ae32, ref);
        }
        if (s_n % 4 != 0) {
            bench_nota("%dx%d no es multiplo de 4: aes3 usa la ruta ae32, sin SIMD.", s_n, s_n);
        }
    }
}
