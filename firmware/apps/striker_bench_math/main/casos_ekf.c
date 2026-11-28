/*
 * Caso del EKF: un paso completo (prediccion + dos correcciones) en float.
 *
 * Estado (6): x, y, theta (mundo) y vx, vy, w (cuerpo).
 *   Prediccion: modelo de velocidad constante, dt = 10 ms (periodo de T_nav).
 *   Correccion 1: odometria de las ruedas (vx, vy, w medidos), 3 medidas.
 *   Correccion 2: rumbo de la IMU (theta medido), 1 medida.
 *
 * Tres variantes que hacen las mismas cuentas y solo cambian los productos de
 * matrices (P = F P F' y P -= K H P):
 *   - C simple con matrices de 6x6.
 *   - esp-dsp (dspm_mult_f32_aes3) con matrices de 6x6: como 6 no es multiplo
 *     de 4, usa la ruta sin SIMD.
 *   - esp-dsp con las matrices rellenadas con ceros hasta 8x8: usa el SIMD,
 *     pero hace mas operaciones (512 frente a 216 por producto).
 * Las tres deben dar el mismo resultado (se revisa antes de medir).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_dsp.h"

#include "bench.h"

#define NE      6 /* numero de estados */
#define LD_MAX  8 /* dimension guardada maxima (con relleno) */
#define DT      0.01f
#define N_MED   256 /* medidas simuladas (potencia de 2) */
#define R_ODO   0.0025f /* varianza de la odometria: (0,05 m/s)^2 */
#define R_TH    0.0003f /* varianza del rumbo: (1 grado)^2 */

static const float Q_DIAG[NE] = {1e-6f, 1e-6f, 1e-6f, 1e-3f, 1e-3f, 1e-3f};

typedef void (*mult_fn_t)(const float *a, const float *b, float *c, int m, int n, int k);

typedef struct {
    int ld; /* 6, o 8 con relleno de ceros */
    mult_fn_t mult;
    float x[NE];
    float P[LD_MAX * LD_MAX] __attribute__((aligned(16)));
    float F[LD_MAX * LD_MAX] __attribute__((aligned(16)));
    float Ft[LD_MAX * LD_MAX] __attribute__((aligned(16)));
    float T[LD_MAX * LD_MAX] __attribute__((aligned(16)));
    float K[LD_MAX * 4] __attribute__((aligned(16)));   /* NE x 3 (o 8 x 4) */
    float HP[4 * LD_MAX] __attribute__((aligned(16)));  /* 3 x NE (o 4 x 8) */
    float KHP[LD_MAX * LD_MAX] __attribute__((aligned(16)));
} ekf_t;

#define M(A, i, j) ((A)[(i) * e->ld + (j)])

static float s_z_odo[N_MED][3];
static float s_z_th[N_MED];
static float s_x_real[N_MED][2];

static ekf_t s_ekf_c, s_ekf_d6, s_ekf_d8;

/* Productos: A (m x n) por B (n x k) = C (m x k), todo por filas. */
static void mult_c(const float *a, const float *b, float *c, int m, int n, int k)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < k; j++) {
            float suma = 0.0f;
            for (int t = 0; t < n; t++) {
                suma += a[i * n + t] * b[t * k + j];
            }
            c[i * k + j] = suma;
        }
    }
}

static void mult_dsp(const float *a, const float *b, float *c, int m, int n, int k)
{
    dspm_mult_f32_aes3(a, b, c, m, n, k);
}

static float angulo_pi(float a)
{
    while (a > (float)M_PI) {
        a -= 2.0f * (float)M_PI;
    }
    while (a < -(float)M_PI) {
        a += 2.0f * (float)M_PI;
    }
    return a;
}

static void inversa3(const float s[9], float r[9])
{
    float c00 = s[4] * s[8] - s[5] * s[7];
    float c01 = s[5] * s[6] - s[3] * s[8];
    float c02 = s[3] * s[7] - s[4] * s[6];
    float inv_det = 1.0f / (s[0] * c00 + s[1] * c01 + s[2] * c02);
    r[0] = c00 * inv_det;
    r[1] = (s[2] * s[7] - s[1] * s[8]) * inv_det;
    r[2] = (s[1] * s[5] - s[2] * s[4]) * inv_det;
    r[3] = c01 * inv_det;
    r[4] = (s[0] * s[8] - s[2] * s[6]) * inv_det;
    r[5] = (s[2] * s[3] - s[0] * s[5]) * inv_det;
    r[6] = c02 * inv_det;
    r[7] = (s[1] * s[6] - s[0] * s[7]) * inv_det;
    r[8] = (s[0] * s[4] - s[1] * s[3]) * inv_det;
}

static void ekf_iniciar(ekf_t *e, int ld, mult_fn_t mult)
{
    memset(e, 0, sizeof(*e));
    e->ld = ld;
    e->mult = mult;
    for (int i = 0; i < NE; i++) {
        M(e->F, i, i) = 1.0f; /* el relleno (filas y columnas 6 y 7) queda en 0 */
        M(e->Ft, i, i) = 1.0f;
        M(e->P, i, i) = (i < 3) ? 0.01f : 0.1f;
    }
    M(e->F, 2, 5) = DT;
    M(e->Ft, 5, 2) = DT;
}

static __attribute__((noinline)) void ekf_paso(ekf_t *e, const float z_odo[3], float z_th)
{
    const int ld = e->ld;
    const int kc = (ld == 8) ? 4 : 3; /* columnas de K y filas de HP */
    float *x = e->x;

    /* --- Prediccion --- */
    float c = cosf(x[2]);
    float s = sinf(x[2]);
    float vx = x[3];
    float vy = x[4];
    x[0] += (vx * c - vy * s) * DT;
    x[1] += (vx * s + vy * c) * DT;
    x[2] += x[5] * DT;
    /* F = I + dt * df/dx: solo cambian 6 elementos (F(2,5) = dt es fijo). */
    M(e->F, 0, 2) = (-vx * s - vy * c) * DT;
    M(e->F, 0, 3) = c * DT;
    M(e->F, 0, 4) = -s * DT;
    M(e->F, 1, 2) = (vx * c - vy * s) * DT;
    M(e->F, 1, 3) = s * DT;
    M(e->F, 1, 4) = c * DT;
    M(e->Ft, 2, 0) = M(e->F, 0, 2);
    M(e->Ft, 3, 0) = M(e->F, 0, 3);
    M(e->Ft, 4, 0) = M(e->F, 0, 4);
    M(e->Ft, 2, 1) = M(e->F, 1, 2);
    M(e->Ft, 3, 1) = M(e->F, 1, 3);
    M(e->Ft, 4, 1) = M(e->F, 1, 4);
    e->mult(e->F, e->P, e->T, ld, ld, ld);  /* T = F P */
    e->mult(e->T, e->Ft, e->P, ld, ld, ld); /* P = T F' */
    for (int i = 0; i < NE; i++) {
        M(e->P, i, i) += Q_DIAG[i];
    }

    /* --- Correccion con la odometria: H elige los estados 3, 4 y 5 --- */
    float S[9];
    float Si[9];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            S[i * 3 + j] = M(e->P, 3 + i, 3 + j) + ((i == j) ? R_ODO : 0.0f);
        }
    }
    inversa3(S, Si);
    for (int i = 0; i < NE; i++) { /* K = P H' S^-1 */
        for (int j = 0; j < 3; j++) {
            e->K[i * kc + j] = M(e->P, i, 3) * Si[j] + M(e->P, i, 4) * Si[3 + j] + M(e->P, i, 5) * Si[6 + j];
        }
    }
    float y0 = z_odo[0] - x[3];
    float y1 = z_odo[1] - x[4];
    float y2 = z_odo[2] - x[5];
    for (int i = 0; i < NE; i++) {
        x[i] += e->K[i * kc] * y0 + e->K[i * kc + 1] * y1 + e->K[i * kc + 2] * y2;
    }
    for (int i = 0; i < 3; i++) { /* H P = filas 3, 4 y 5 de P */
        for (int j = 0; j < NE; j++) {
            e->HP[i * ld + j] = M(e->P, 3 + i, j);
        }
    }
    e->mult(e->K, e->HP, e->KHP, ld, kc, ld); /* K H P */
    for (int i = 0; i < NE; i++) {
        for (int j = 0; j < NE; j++) {
            M(e->P, i, j) -= M(e->KHP, i, j);
        }
    }

    /* --- Correccion con el rumbo de la IMU: H elige el estado 2 --- */
    float inv_s = 1.0f / (M(e->P, 2, 2) + R_TH);
    float kth[NE];
    float fila2[NE];
    for (int i = 0; i < NE; i++) {
        kth[i] = M(e->P, i, 2) * inv_s;
        fila2[i] = M(e->P, 2, i);
    }
    float yth = angulo_pi(z_th - x[2]);
    for (int i = 0; i < NE; i++) {
        x[i] += kth[i] * yth;
    }
    for (int i = 0; i < NE; i++) {
        for (int j = 0; j < NE; j++) {
            M(e->P, i, j) -= kth[i] * fila2[j];
        }
    }
}

/* Trayectoria simulada: circulo de 0,5 m de radio a 0,3 m/s, con ruido. */
static void simular_medidas(void)
{
    float x = 0.0f, y = 0.0f, th = 0.0f;
    const float vx = 0.3f, vy = 0.0f, w = 0.6f;
    for (int k = 0; k < N_MED; k++) {
        x += (vx * cosf(th) - vy * sinf(th)) * DT;
        y += (vx * sinf(th) + vy * cosf(th)) * DT;
        th += w * DT;
        s_x_real[k][0] = x;
        s_x_real[k][1] = y;
        s_z_odo[k][0] = vx + bench_azar_f(-0.05f, 0.05f);
        s_z_odo[k][1] = vy + bench_azar_f(-0.05f, 0.05f);
        s_z_odo[k][2] = w + bench_azar_f(-0.05f, 0.05f);
        s_z_th[k] = angulo_pi(th + bench_azar_f(-0.02f, 0.02f));
    }
}

/* Corre las tres variantes sobre las mismas medidas y compara x y P. */
static void revisar_variantes(void)
{
    ekf_iniciar(&s_ekf_c, NE, mult_c);
    ekf_iniciar(&s_ekf_d6, NE, mult_dsp);
    ekf_iniciar(&s_ekf_d8, LD_MAX, mult_dsp);
    float error_pos = 0.0f;
    for (int k = 0; k < N_MED; k++) {
        ekf_paso(&s_ekf_c, s_z_odo[k], s_z_th[k]);
        ekf_paso(&s_ekf_d6, s_z_odo[k], s_z_th[k]);
        ekf_paso(&s_ekf_d8, s_z_odo[k], s_z_th[k]);
        float dx = s_ekf_c.x[0] - s_x_real[k][0];
        float dy = s_ekf_c.x[1] - s_x_real[k][1];
        error_pos += dx * dx + dy * dy;
    }
    float dif = 0.0f;
    for (int i = 0; i < NE; i++) {
        dif = fmaxf(dif, fabsf(s_ekf_d6.x[i] - s_ekf_c.x[i]));
        dif = fmaxf(dif, fabsf(s_ekf_d8.x[i] - s_ekf_c.x[i]));
        for (int j = 0; j < NE; j++) {
            dif = fmaxf(dif, fabsf(s_ekf_d6.P[i * NE + j] - s_ekf_c.P[i * NE + j]));
            dif = fmaxf(dif, fabsf(s_ekf_d8.P[i * LD_MAX + j] - s_ekf_c.P[i * NE + j]));
        }
    }
    bench_nota("Las 3 variantes, %d pasos: diferencia maxima en x y P = %.2e (%s).", N_MED,
               (double)dif, (dif < 1e-4f) ? "coinciden" : "ERROR: no coinciden");
    bench_nota("Error RMS de posicion frente a la trayectoria simulada: %.1f mm.",
               (double)(1000.0f * sqrtf(error_pos / N_MED)));
}

static void caso_ekf_c(int n)
{
    for (int i = 0; i < n; i++) {
        ekf_paso(&s_ekf_c, s_z_odo[i & (N_MED - 1)], s_z_th[i & (N_MED - 1)]);
    }
    bench_sumidero_f = s_ekf_c.x[0];
}

static void caso_ekf_d6(int n)
{
    for (int i = 0; i < n; i++) {
        ekf_paso(&s_ekf_d6, s_z_odo[i & (N_MED - 1)], s_z_th[i & (N_MED - 1)]);
    }
    bench_sumidero_f = s_ekf_d6.x[0];
}

static void caso_ekf_d8(int n)
{
    for (int i = 0; i < n; i++) {
        ekf_paso(&s_ekf_d8, s_z_odo[i & (N_MED - 1)], s_z_th[i & (N_MED - 1)]);
    }
    bench_sumidero_f = s_ekf_d8.x[0];
}

void casos_ekf(void)
{
    simular_medidas();
    bench_grupo("EKF de 6 estados: un paso (T_nav, cada 10 ms)");
    revisar_variantes();
    float ref = bench_medir(caso_ekf_c);
    bench_fila("prediccion + 2 correcciones", "C simple, 6x6", ref, ref);
    bench_fila("prediccion + 2 correcciones", "esp-dsp, 6x6 (sin SIMD)", bench_medir(caso_ekf_d6), ref);
    bench_fila("prediccion + 2 correcciones", "esp-dsp, rellenado a 8x8", bench_medir(caso_ekf_d8), ref);
}
