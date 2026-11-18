#include "senfusion.h"

void senfusion_init(senfusion_t *f, const senfusion_config_t *cfg)
{
    // Start with zero estimate and high uncertainty
    f->x = 0.0f;
    f->P = 1e3f;  // choose large P0 if you want to trust first measurements

    // Copy model & noise parameters
    f->A  = cfg->A;
    f->B  = cfg->B;
    f->Q  = cfg->Q;
    f->R1 = cfg->R1;
    f->R2 = cfg->R2;
}

void senfusion_predict(senfusion_t *f, float u)
{
    // State prediction: x = A*x + B*u
    f->x = f->A * f->x + f->B * u;

    // Uncertainty propagation: P = A*P*A + Q
    f->P = f->A * f->P * f->A + f->Q;
}

static inline void _kalman_update_scalar(float *x, float *P, float z, float R)
{
    // Compute gain K = P / (P + R)
    float K = (*P) / ((*P) + R);

    // Update state
    *x = *x + K * (z - *x);

    // Update covariance
    *P = (1.0f - K) * (*P);
}

void senfusion_update1(senfusion_t *f, float z1)
{
    // 1) Update with first measurement z1 (e.g. wheel velocity)
    _kalman_update_scalar(&f->x, &f->P, z1, f->R1);
}

void senfusion_update2(senfusion_t *f, float z2)
{
    // 2) Update with second measurement z2 (e.g. IMU-derived velocity)
    _kalman_update_scalar(&f->x, &f->P, z2, f->R2);
}

///<-------------------------------------------------------------
///<--------------- LINEAR ALGEBRA FUNCTIONS --------------------
///<-------------------------------------------------------------

void matrixMultiplication(int rowsA, int colsA, int colsB, float A[rowsA][colsA], float B[colsA][colsB], float C[rowsA][colsB]) {
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsB; j++) {
            C[i][j] = 0;  // Initialize each element of C
            for (int k = 0; k < colsA; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void matrixAddSub(int rows, int cols, float A[rows][cols], float B[rows][cols], float C[rows][cols], char operation) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (operation == '+') {
                C[i][j] = A[i][j] + B[i][j];  // Add of matrices
            } else if (operation == '-') {
                C[i][j] = A[i][j] - B[i][j];  // Sub of matrices
            } else {
                printf("Invalid operation. Use '+' to add or '-' to sub.\n");
                return;
            }
        }
    }
}

void matrixTranspose(int rows, int cols, float A[rows][cols], float B[cols][rows]) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            B[j][i] = A[i][j];  // Transpose of matrix
        }
    }
}

/// <-------------------------------------------------------------
/// <------------------ LU DECOMPOSITION -------------------------
/// <-------------------------------------------------------------

#define N 3       // Size of matrix A (NxN)
#define M 2       // Number of columns in B (right-hand sides)

void LU_decomposition(float A[N][N], float L[N][N], float U[N][N]) {
    for (int i = 0; i < N; i++) {
        // Compute U matrix
        for (int k = i; k < N; k++) {
            float sum = 0.0;
            for (int j = 0; j < i; j++)
                sum += L[i][j] * U[j][k];
            U[i][k] = A[i][k] - sum;
        }

        // Compute L matrix
        for (int k = i; k < N; k++) {
            if (i == k)
                L[i][i] = 1.0;
            else {
                float sum = 0.0;
                for (int j = 0; j < i; j++)
                    sum += L[k][j] * U[j][i];
                L[k][i] = (A[k][i] - sum) / U[i][i];
            }
        }
    }
}

void forward_substitution(float L[N][N], float b[N], float y[N]) { // L·y = b
    for (int i = 0; i < N; i++) {
        float s = b[i];
        for (int j = 0; j < i; j++) s -= L[i][j] * y[j];
        y[i] = s;  // L[i][i] == 1
    }
}

void backward_substitution(float U[N][N], float y[N], float x[N]) { // U·x = y
    for (int i = N - 1; i >= 0; i--) {
        float s = y[i];
        for (int j = i + 1; j < N; j++) s -= U[i][j] * x[j];
        x[i] = s / U[i][i];
    }
}

void solve_LU_system(float A[N][N], float B[M][N], float X[M][N]) {
    float A_T[N][N];
    float L[N][N] = {0}, U[N][N] = {0};

    // 1) Transpose A to get A_T = A^T
    matrixTranspose(N, N, A, A_T);

    // 2) Factorize A_T into L and U: A_T = L * U
    LU_decomposition(A_T, L, U);

    // 3) For each row i of B, solve (A^T) * z = b, then set X[i,:] = z^T
    for (int i = 0; i < M; i++) {
        float b[N], y[N], z[N];

        // 3a) Build the right-hand side vector b = B[i,:]^T
        for (int k = 0; k < N; k++) {
            b[k] = B[i][k];
        }

        // 3b) Forward substitution: L * y = b
        forward_substitution(L, b, y);

        // 3c) Backward substitution: U * z = y
        backward_substitution(U, y, z);

        // 3d) Store the solution z^T into the i-th row of X
        for (int j = 0; j < N; j++) {
            X[i][j] = z[j];
        }
    }
}

void kalman1D_init(kalman1D_t *kf, float Q, float R)
{
    kf->x = 0.0f;  // Initial estimated value
    kf->P = 1.0f;  // Initial estimation error covariance
    kf->Q = Q;     // Process noise covariance
    kf->R = R;     // Measurement noise covariance
}

float kalman1D_update(kalman1D_t *kf, float meas)
{
    // Prediction step
    kf->P += kf->Q;  // Update estimation error covariance

    // Measurement update step
    float K = kf->P / (kf->P + kf->R);  // Kalman gain
    kf->x += K * (meas - kf->x);  // Update estimated value
    kf->P *= (1 - K);  // Update estimation error covariance

    return kf->x;  // Return the estimated value
}

void calc_invkinematics(float vbx, float vby, float wb, float *w1, float *w2, float *w3)
{
    *w1 = (-vbx*sin(DELTA) - vby*cos(DELTA) + ROBOT_RADIUS*wb) / WHEEL_RADIUS;
    *w2 = (vbx + ROBOT_RADIUS * wb) / WHEEL_RADIUS;
    *w3 = (-vbx*sin(DELTA) + vby*cos(DELTA) + ROBOT_RADIUS*wb) / WHEEL_RADIUS;
}

void calc_fordwkinematics(float w1, float w2, float w3, float *vbx, float *vby, float *wb)
{
    // Precompute trigonometric terms
    float s = sinf(DELTA);
    float c = cosf(DELTA);
    float denom_xy = 2.0f * (1.0f + s);

    // Linear velocity in body x-direction
    *vbx = WHEEL_RADIUS * (-w1 + 2.0f * w2 - w3) / denom_xy;

    // Linear velocity in body y-direction
    *vby = WHEEL_RADIUS * (-w1 + w3) / (2.0f * c);

    // Angular velocity about body center (positive CCW)
    *wb  = WHEEL_RADIUS * (w1 + 2.0f * s * w2 + w3) / (2.0f * ROBOT_RADIUS * (1.0f + s));
}

void test_forward_kinematics(int num_tests)
{
    srand((unsigned)time(NULL));
    int failures = 0;

    for (int i = 0; i < num_tests; ++i) {
        // Random velocities in some suitable range, e.g. [-1, 1]
        float vbx = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float vby = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float wb  = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

        float w1, w2, w3;
        calc_invkinematics(vbx, vby, wb, &w1, &w2, &w3);

        float vbx_r, vby_r, wb_r;
        calc_fordwkinematics(w1, w2, w3, &vbx_r, &vby_r, &wb_r);

        if (fabsf(vbx - vbx_r) > TOLERANCE ||
            fabsf(vby - vby_r) > TOLERANCE ||
            fabsf(wb  - wb_r) > TOLERANCE) {
            failures++;
            printf("Test %d failed:\n"
                   "  Original: vbx=%.6f, vby=%.6f, wb=%.6f\n"
                   "  Recovered: vbx=%.6f, vby=%.6f, wb=%.6f\n",
                   i, vbx, vby, wb, vbx_r, vby_r, wb_r);
        }
    }

    if (failures == 0) {
        printf("All %d forward-kinematics tests passed.\n", num_tests);
    } else {
        printf("%d out of %d tests failed.\n", failures, num_tests);
    }
}
