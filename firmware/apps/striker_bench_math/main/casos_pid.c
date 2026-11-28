/*
 * Casos del lazo de ruedas: PID y filtro del encoder, en double, float y Q16.16.
 *
 * El PID es la forma discreta de pid_ext que usa el banco de autotuning
 * (cal_type PID_CAL_TYPE_DISCRETE, beta 0,4, sample_time 1, salida en +-80):
 * firmware/components/control/pid_lib/autotuning_rnn/pid_ext.c, funcion
 * pid_calc_discrete. El filtro del encoder es el de velocidad del banco
 * (beta = exp(-2*pi/100)), con el angulo en cuentas del AS5600 (0..4095).
 *
 * Q16.16: entero de 32 bits con 16 bits enteros y 16 fraccionarios
 * (resolucion 1/65536 = 1,5e-5).
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "bench.h"

#define N_DATOS 256 /* potencia de 2: el indice usa & (N_DATOS - 1) */
#define MASCARA (N_DATOS - 1)

/* Parametros del banco de autotuning */
#define BETA_PID 0.4
#define TS_PID   1.0
#define U_MAX    80.0
/* Ganancias de ejemplo, del orden de las del banco */
#define KP 2.7
#define KI 0.0025
#define KD 0.0025

/* Encoder: AS5600 de 12 bits leido cada 1 ms */
#define CUENTAS 4096
#define TS_ENC  0.001

typedef int32_t q16_t;
#define Q16_UNO 65536

static inline q16_t a_q16(double x)
{
    return (q16_t)llround(x * Q16_UNO);
}

static inline double de_q16(q16_t x)
{
    return (double)x / Q16_UNO;
}

static inline q16_t mul_q16(q16_t a, q16_t b)
{
    return (q16_t)(((int64_t)a * b + (1 << 15)) >> 16);
}

/* ------------------------------------------------------------------------- */
/* PID                                                                        */
/* ------------------------------------------------------------------------- */

/* pid_ext tal cual: recalcula los coeficientes (y 5 divisiones) en cada paso. */
typedef struct {
    float kp, ki, kd, beta, ts, umax, umin;
    float e1, e2, u1, u2;
} pid_ext_t;

static __attribute__((noinline)) float pid_ext_paso(pid_ext_t *p, float e)
{
    float b0 = (p->kp * (1 + p->beta * p->ts)) + (p->ki * p->ts * (1 + p->beta * p->ts)) + (p->kd * p->beta);
    float b1 = (-p->kp * (2 + p->beta * p->ts)) + (p->ki * p->ts) + (p->kd * (2 * p->beta));
    float b2 = p->kp + (p->kd * p->beta);
    float a0 = 1 + p->beta * p->ts;
    float a1 = -(2 + p->beta * p->ts);
    float a2 = 1;
    float u = (b0 / a0) * e + (b1 / a0) * p->e1 + (b2 / a0) * p->e2 - (a1 / a0) * p->u1 - (a2 / a0) * p->u2;
    u = (u < p->umax) ? u : p->umax;
    u = (u > p->umin) ? u : p->umin;
    p->e2 = p->e1;
    p->e1 = e;
    p->u2 = p->u1;
    p->u1 = u;
    return u;
}

/* Misma ecuacion con los coeficientes calculados una sola vez:
 * u = c0*e + c1*e1 + c2*e2 + d1*u1 + d2*u2, saturada en +-umax. */
typedef struct {
    double c0, c1, c2, d1, d2, umax;
    double e1, e2, u1, u2;
} pid_d_t;

typedef struct {
    float c0, c1, c2, d1, d2, umax;
    float e1, e2, u1, u2;
} pid_f_t;

typedef struct {
    q16_t c0, c1, c2, d1, d2, umax;
    q16_t e1, e2, u1, u2;
} pid_q_t;

static __attribute__((noinline)) double pid_d_paso(pid_d_t *p, double e)
{
    double u = p->c0 * e + p->c1 * p->e1 + p->c2 * p->e2 + p->d1 * p->u1 + p->d2 * p->u2;
    u = (u < p->umax) ? u : p->umax;
    u = (u > -p->umax) ? u : -p->umax;
    p->e2 = p->e1;
    p->e1 = e;
    p->u2 = p->u1;
    p->u1 = u;
    return u;
}

static __attribute__((noinline)) float pid_f_paso(pid_f_t *p, float e)
{
    float u = p->c0 * e + p->c1 * p->e1 + p->c2 * p->e2 + p->d1 * p->u1 + p->d2 * p->u2;
    u = (u < p->umax) ? u : p->umax;
    u = (u > -p->umax) ? u : -p->umax;
    p->e2 = p->e1;
    p->e1 = e;
    p->u2 = p->u1;
    p->u1 = u;
    return u;
}

static __attribute__((noinline)) q16_t pid_q_paso(pid_q_t *p, q16_t e)
{
    /* Productos de 32x32 bits en 64 bits: no hay desborde con |e| < 32768. */
    int64_t acc = (int64_t)p->c0 * e + (int64_t)p->c1 * p->e1 + (int64_t)p->c2 * p->e2 +
                  (int64_t)p->d1 * p->u1 + (int64_t)p->d2 * p->u2;
    q16_t u = (q16_t)((acc + (1 << 15)) >> 16);
    u = (u < p->umax) ? u : p->umax;
    u = (u > -p->umax) ? u : -p->umax;
    p->e2 = p->e1;
    p->e1 = e;
    p->u2 = p->u1;
    p->u1 = u;
    return u;
}

/* Coeficientes de pid_calc_discrete, divididos por a0. */
static void coeficientes(double c[5])
{
    double a0 = 1 + BETA_PID * TS_PID;
    double a1 = -(2 + BETA_PID * TS_PID);
    double a2 = 1;
    double b0 = KP * (1 + BETA_PID * TS_PID) + KI * TS_PID * (1 + BETA_PID * TS_PID) + KD * BETA_PID;
    double b1 = -KP * (2 + BETA_PID * TS_PID) + KI * TS_PID + KD * (2 * BETA_PID);
    double b2 = KP + KD * BETA_PID;
    c[0] = b0 / a0;
    c[1] = b1 / a0;
    c[2] = b2 / a0;
    c[3] = -a1 / a0;
    c[4] = -a2 / a0;
}

static void iniciar_pids(pid_ext_t *ext, pid_d_t *d, pid_f_t *f, pid_q_t *q)
{
    double c[5];
    coeficientes(c);
    *ext = (pid_ext_t){(float)KP, (float)KI, (float)KD, (float)BETA_PID, (float)TS_PID,
                       (float)U_MAX, (float)-U_MAX, 0, 0, 0, 0};
    *d = (pid_d_t){c[0], c[1], c[2], c[3], c[4], U_MAX, 0, 0, 0, 0};
    *f = (pid_f_t){(float)c[0], (float)c[1], (float)c[2], (float)c[3], (float)c[4], (float)U_MAX, 0, 0, 0, 0};
    /* Redondeo que conserva las sumas: d1 + d2 = 1 exacto, para que el polo del
     * integrador quede en z = 1 (si no, la salida deriva), y c0 + c1 + c2 queda
     * a menos de medio bit de la ganancia integral real (~0,007 por paso, muy
     * pequena frente a la resolucion de 1/65536). */
    q16_t c0 = a_q16(c[0]);
    q16_t c1 = a_q16(c[1]);
    q16_t c2 = a_q16(c[0] + c[1] + c[2]) - c0 - c1;
    q16_t d1 = a_q16(c[3]);
    *q = (pid_q_t){c0, c1, c2, d1, Q16_UNO - d1, a_q16(U_MAX), 0, 0, 0, 0};
}

/* ------------------------------------------------------------------------- */
/* Filtro y derivada del encoder                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
    int32_t previa;
    double wf;
} enc_d_t;

typedef struct {
    int32_t previa;
    float wf;
} enc_f_t;

typedef struct {
    int32_t previa;
    q16_t wf;
} enc_q_t;

static double s_kw_d, s_beta_d;
static float s_kw_f, s_beta_f;
static q16_t s_kw_q, s_beta_q, s_1mbeta_q;

/* Diferencia de cuentas con desenrollado (el angulo pasa de 4095 a 0). */
static inline int32_t delta_cuentas(int32_t *previa, int32_t cruda)
{
    int32_t d = cruda - *previa;
    *previa = cruda;
    if (d > CUENTAS / 2) {
        d -= CUENTAS;
    } else if (d < -CUENTAS / 2) {
        d += CUENTAS;
    }
    return d;
}

static __attribute__((noinline)) double enc_d_paso(enc_d_t *e, int32_t cruda)
{
    double w = delta_cuentas(&e->previa, cruda) * s_kw_d;
    e->wf = s_beta_d * e->wf + (1.0 - s_beta_d) * w;
    return e->wf;
}

static __attribute__((noinline)) float enc_f_paso(enc_f_t *e, int32_t cruda)
{
    float w = (float)delta_cuentas(&e->previa, cruda) * s_kw_f;
    e->wf = s_beta_f * e->wf + (1.0f - s_beta_f) * w;
    return e->wf;
}

static __attribute__((noinline)) q16_t enc_q_paso(enc_q_t *e, int32_t cruda)
{
    /* |d| <= 2048 y s_kw_q ~ 100 531: el producto cabe en 32 bits. */
    q16_t w = delta_cuentas(&e->previa, cruda) * s_kw_q;
    e->wf = mul_q16(s_beta_q, e->wf) + mul_q16(s_1mbeta_q, w);
    return e->wf;
}

static void iniciar_filtros(void)
{
    s_kw_d = 2.0 * M_PI / CUENTAS / TS_ENC; /* rad/s por cuenta en un periodo */
    s_beta_d = exp(-2.0 * M_PI / 100.0);
    s_kw_f = (float)s_kw_d;
    s_beta_f = (float)s_beta_d;
    s_kw_q = a_q16(s_kw_d);
    s_beta_q = a_q16(s_beta_d);
    s_1mbeta_q = Q16_UNO - s_beta_q; /* ganancia en continua exactamente 1 */
}

/* ------------------------------------------------------------------------- */
/* Datos de prueba y casos                                                    */
/* ------------------------------------------------------------------------- */

static double s_err_d[N_DATOS];
static float s_err_f[N_DATOS];
static q16_t s_err_q[N_DATOS];
static int32_t s_cuentas[N_DATOS];

static pid_ext_t s_ext;
static pid_d_t s_pd;
static pid_f_t s_pf;
static pid_q_t s_pq;
static enc_d_t s_ed;
static enc_f_t s_ef;
static enc_q_t s_eq;

static void preparar_datos(void)
{
    double fase = 0.0;
    for (int i = 0; i < N_DATOS; i++) {
        double e = 6.0 * sin(2.0 * M_PI * i / N_DATOS) + bench_azar_f(-0.5f, 0.5f);
        s_err_d[i] = e;
        s_err_f[i] = (float)e;
        s_err_q[i] = a_q16(e);
        /* Rueda a unos 10 rad/s: ~6,5 cuentas por milisegundo, con ruido. */
        fase += 6.5 + 3.0 * sin(2.0 * M_PI * i / N_DATOS) + bench_azar_f(-1.5f, 1.5f);
        s_cuentas[i] = ((int32_t)llround(fase)) & (CUENTAS - 1);
    }
}

static void caso_pid_ext(int n)
{
    float u = 0.0f;
    for (int i = 0; i < n; i++) {
        u = pid_ext_paso(&s_ext, s_err_f[i & MASCARA]);
    }
    bench_sumidero_f = u;
}

static void caso_pid_d(int n)
{
    double u = 0.0;
    for (int i = 0; i < n; i++) {
        u = pid_d_paso(&s_pd, s_err_d[i & MASCARA]);
    }
    bench_sumidero_d = u;
}

static void caso_pid_f(int n)
{
    float u = 0.0f;
    for (int i = 0; i < n; i++) {
        u = pid_f_paso(&s_pf, s_err_f[i & MASCARA]);
    }
    bench_sumidero_f = u;
}

static void caso_pid_q(int n)
{
    q16_t u = 0;
    for (int i = 0; i < n; i++) {
        u = pid_q_paso(&s_pq, s_err_q[i & MASCARA]);
    }
    bench_sumidero_i = u;
}

static void caso_enc_d(int n)
{
    double w = 0.0;
    for (int i = 0; i < n; i++) {
        w = enc_d_paso(&s_ed, s_cuentas[i & MASCARA]);
    }
    bench_sumidero_d = w;
}

static void caso_enc_f(int n)
{
    float w = 0.0f;
    for (int i = 0; i < n; i++) {
        w = enc_f_paso(&s_ef, s_cuentas[i & MASCARA]);
    }
    bench_sumidero_f = w;
}

static void caso_enc_q(int n)
{
    q16_t w = 0;
    for (int i = 0; i < n; i++) {
        w = enc_q_paso(&s_eq, s_cuentas[i & MASCARA]);
    }
    bench_sumidero_i = w;
}

/* Error de cuantizacion: la misma secuencia por las tres versiones; el error
 * se mide frente a double. */
static void error_cuantizacion(void)
{
    const int pasos = 20000;
    pid_ext_t ext;
    pid_d_t d;
    pid_f_t f;
    pid_q_t q;
    iniciar_pids(&ext, &d, &f, &q);

    double sf = 0, sq = 0, mf = 0, mq = 0, su = 0;
    for (int k = 0; k < pasos; k++) {
        double t = k * 0.001;
        /* Error tipico en lazo cerrado: pocos rad/s, sin llevar la salida a
         * saturacion (si satura, cualquier redondeo cambia cuando lo hace). */
        double e = 2.0 * sin(2.0 * M_PI * 0.5 * t) + 0.5 * sin(2.0 * M_PI * 7.0 * t) + bench_azar_f(-0.3f, 0.3f);
        double ud = pid_d_paso(&d, e);
        double ef = (double)pid_f_paso(&f, (float)e) - ud;
        double eq = de_q16(pid_q_paso(&q, a_q16(e))) - ud;
        sf += ef * ef;
        sq += eq * eq;
        su += ud * ud;
        mf = fmax(mf, fabs(ef));
        mq = fmax(mq, fabs(eq));
    }
    printf("\n[Error de cuantizacion frente a double]\n");
    printf("PID, %d pasos (salida en +-80, RMS de la salida %.2f):\n", pasos, sqrt(su / pasos));
    printf("  float : RMS %.2e, maximo %.2e\n", sqrt(sf / pasos), mf);
    printf("  Q16.16: RMS %.2e, maximo %.2e\n", sqrt(sq / pasos), mq);

    enc_d_t ed = {0, 0.0};
    enc_f_t ef = {0, 0.0f};
    enc_q_t eq = {0, 0};
    double fase = 0.0;
    sf = sq = mf = mq = su = 0;
    for (int k = 0; k < pasos; k++) {
        double t = k * 0.001;
        /* Velocidad entre -15 y 15 rad/s, con ruido de +-2 cuentas. */
        double w_real = 15.0 * sin(2.0 * M_PI * 0.25 * t);
        fase += w_real * TS_ENC / (2.0 * M_PI) * CUENTAS;
        int32_t cruda = ((int32_t)llround(fase + bench_azar_f(-2.0f, 2.0f))) & (CUENTAS - 1);
        double wd = enc_d_paso(&ed, cruda);
        double e1 = (double)enc_f_paso(&ef, cruda) - wd;
        double e2 = de_q16(enc_q_paso(&eq, cruda)) - wd;
        if (k > 0) { /* el primer paso arranca desde la cuenta 0 */
            sf += e1 * e1;
            sq += e2 * e2;
            su += wd * wd;
            mf = fmax(mf, fabs(e1));
            mq = fmax(mq, fabs(e2));
        }
    }
    printf("Filtro del encoder, %d pasos (rad/s, RMS de la velocidad %.2f):\n", pasos, sqrt(su / (pasos - 1)));
    printf("  float : RMS %.2e, maximo %.2e\n", sqrt(sf / (pasos - 1)), mf);
    printf("  Q16.16: RMS %.2e, maximo %.2e\n", sqrt(sq / (pasos - 1)), mq);
}

void casos_pid(void)
{
    preparar_datos();
    iniciar_filtros();
    iniciar_pids(&s_ext, &s_pd, &s_pf, &s_pq);

    bench_grupo("PID por rueda: un paso (T_ctrl hace 3 por ms)");
    float ref = bench_medir(caso_pid_d);
    bench_fila("PID discreto, coef. fijos", "double (por software)", ref, ref);
    bench_fila("PID discreto, coef. fijos", "float (FPU)", bench_medir(caso_pid_f), ref);
    bench_fila("PID discreto, coef. fijos", "Q16.16 (enteros)", bench_medir(caso_pid_q), ref);
    bench_fila("pid_ext tal cual", "float, coef. en cada paso", bench_medir(caso_pid_ext), ref);
    bench_nota("pid_ext recalcula sus coeficientes (con 5 divisiones) en cada paso.");

    bench_grupo("Filtro y derivada del encoder: un paso por rueda");
    ref = bench_medir(caso_enc_d);
    bench_fila("desenrollado + filtro", "double (por software)", ref, ref);
    bench_fila("desenrollado + filtro", "float (FPU)", bench_medir(caso_enc_f), ref);
    bench_fila("desenrollado + filtro", "Q16.16 (enteros)", bench_medir(caso_enc_q), ref);

    error_cuantizacion();
}
