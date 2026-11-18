#include "control_senfusion.h"

void ctrl_senfusion_init(ctrl_senfusion_t *ctrl_senfusion, pid_block_t pid, float dt)
{
    ///< ---------------- PID CONTROLLER -----------------
    ctrl_senfusion->pid = pid; ///< Set the PID controller
    ctrl_senfusion->pid.prev_err1 = 0; ///< Initialize the previous error
    ctrl_senfusion->pid.prev_err2 = 0; ///< Initialize the previous error
    ctrl_senfusion->pid.prev_u1 = 0; ///< Initialize the previous control output
    ctrl_senfusion->pid.prev_u2 = 0; ///< Initialize the previous control output
    ctrl_senfusion->pid.integral_err = 0; ///< Initialize the integral error

    ///< ----------------- SENSOR FUSION -----------------
    ctrl_senfusion->dt = dt;
    ctrl_senfusion->k = 0;
    ctrl_senfusion->x[0][0] = 0;
    ctrl_senfusion->x[1][0] = 0;
    ctrl_senfusion->z[0][0] = 0;
    ctrl_senfusion->z[1][0] = 0;

    ///< Initialize the process model: x(k+1) = A*x(k) + B*u
    ctrl_senfusion->A[0][0] = 0.9955;
    ctrl_senfusion->A[0][1] = 0.00931;
    ctrl_senfusion->A[1][0] = -0.8805;
    ctrl_senfusion->A[1][1] = 0.8638;
    ctrl_senfusion->B[0][0] = 8.81e-05;
    ctrl_senfusion->B[1][0] = 0.0172;
    ctrl_senfusion->C[0][0] = 1;
    ctrl_senfusion->C[0][1] = 0;
    ctrl_senfusion->C[1][0] = 0;
    ctrl_senfusion->C[1][1] = 1;

    ///< Initialize the measurement model: z(k) = H*x(k) + v(k)
    ctrl_senfusion->H[0][0] = 1; 
    ctrl_senfusion->H[0][1] = 0;
    ctrl_senfusion->H[1][0] = 1;
    ctrl_senfusion->H[1][1] = 0;
    ctrl_senfusion->H[2][0] = 0;
    ctrl_senfusion->H[2][1] = 1;
    ctrl_senfusion->H_T[0][0] = 1;
    ctrl_senfusion->H_T[0][1] = 1;
    ctrl_senfusion->H_T[0][2] = 0;
    ctrl_senfusion->H_T[1][0] = 0;
    ctrl_senfusion->H_T[1][1] = 0;
    ctrl_senfusion->H_T[1][2] = 1;

    ///< Initialize the error covariance matrix: diagonal matrix
    ctrl_senfusion->P[0][0] = 1;
    ctrl_senfusion->P[0][1] = 0;
    ctrl_senfusion->P[1][0] = 0;
    ctrl_senfusion->P[1][1] = 1;

    ///< Initialize the process noise covariance matrix: diagonal matrix
    float sigma_a = 1.0;
    ctrl_senfusion->Q[0][0] = 0.25 * dt * dt * dt * dt * sigma_a;
    ctrl_senfusion->Q[0][1] = 0.5 * dt * dt * dt * sigma_a;
    ctrl_senfusion->Q[1][0] = 0.5 * dt * dt * dt * sigma_a;
    ctrl_senfusion->Q[1][1] = dt * dt * sigma_a;

    ///< Initialize the measurement noise covariance matrix: diagonal matrix
    ///< The covariance for Encoder and Lidar are constant, but the covariance for IMU is time varying
    //< for encoder: sigma_p_enc = 7.84 um
    //< for lidar: sigma_p_lidar = 0.02 m
    //< for IMU: sigma_v_imu = sqrt(k) * sigma_a * dt, sigma_a = 0.0148 m/s^2
    float sigma_p_enc = 0.000784; ///< 7.84 um
    float sigma_p_lidar = 0.02; ///< 2 cm
    float sigma_v_imu = sqrt(ctrl_senfusion->k) * 0.0148 * ctrl_senfusion->dt; ///< sqrt(k) * sigma_a * dt
    ctrl_senfusion->R[0][0] = sigma_p_enc * sigma_p_enc; ///< Encoder
    ctrl_senfusion->R[0][1] = 0;
    ctrl_senfusion->R[1][0] = 0;
    ctrl_senfusion->R[1][1] = sigma_p_lidar * sigma_p_lidar; ///< Lidar
    ctrl_senfusion->R[2][0] = 0;
    ctrl_senfusion->R[2][1] = 0;
    ctrl_senfusion->R[2][2] = sigma_v_imu * sigma_v_imu; ///< IMU
}

void ctrl_senfusion_predict(ctrl_senfusion_t *ctrl_senfusion, float u)
{
    float x_pred[2][1] = {0};
    float P_pred[2][2] = {0};

    ///< Predict the next state: x_pred = A * x + B * u
    matrixMultiplication(2, 2, 1, ctrl_senfusion->A, ctrl_senfusion->x, x_pred); // A * x
    ctrl_senfusion->x_pred[0][0] = x_pred[0][0] + ctrl_senfusion->B[0][0] * u; 
    ctrl_senfusion->x_pred[1][0] = x_pred[1][0] + ctrl_senfusion->B[1][0] * u;

    ///< Predict the next error covariance matrix: P_pred = A * P * Aᵀ + Q
    matrixMultiplication(2, 2, 2, ctrl_senfusion->A, ctrl_senfusion->P, P_pred); // A * P
    float A_transpose[2][2] = {0};
    matrixTranspose(2, 2, ctrl_senfusion->A, A_transpose);
    matrixMultiplication(2, 2, 2, P_pred, A_transpose, ctrl_senfusion->P_pred); // A * P * Aᵀ
    matrixAddSub(2, 2, ctrl_senfusion->P_pred, ctrl_senfusion->Q, ctrl_senfusion->P_pred, '+'); // A * P * Aᵀ + Q

}

void ctrl_senfusion_update(ctrl_senfusion_t *ctrl_senfusion, float p_enc, float p_lidar, float a_imu, uint32_t k)
{
    
    ctrl_senfusion->R[2][2] = sqrt(k) * 0.0148 * ctrl_senfusion->dt; ///< Update the IMU covariance
    ctrl_senfusion->k = k; ///< Update the sample time

    ///< Covariance innovation: S = H * Pₖ|ₖ₋₁ * Hᵀ + R
    float S[3][3] = {0};
    float H_P_pred[3][2] = {0};
    matrixMultiplication(3, 2, 2, ctrl_senfusion->H, ctrl_senfusion->P_pred, H_P_pred); // H * Pₖ|ₖ₋₁
    matrixMultiplication(3, 2, 3, H_P_pred, ctrl_senfusion->H_T, S); // H * Pₖ|ₖ₋₁ * Hᵀ
    matrixAddSub(3, 3, S, ctrl_senfusion->R, S, '+'); // H * Pₖ|ₖ₋₁ * Hᵀ + R
    // print S
    // printf("S: %.4f, %.4f, %.4f\n %.4f, %.4f, %.4f\n %.4f, %.4f, %.4f\n", S[0][0], S[0][1], S[0][2], S[1][0], S[1][1], S[1][2], S[2][0], S[2][1], S[2][2]);

    ///< Kalman gain: K = Pₖ|ₖ₋₁ * Hᵀ * inv(S)  , 2x2 * 2x3 * 3x3 = 2x3
    float K[2][3] = {0};
    float P_pred_H_T[2][3] = {0};
    matrixMultiplication(2, 2, 3, ctrl_senfusion->P_pred, ctrl_senfusion->H_T, P_pred_H_T); // Pₖ|ₖ₋₁ * Hᵀ
    solve_LU_system(S, P_pred_H_T, K); // solve the linear system S * K = Pₖ|ₖ₋₁ * Hᵀ
    // printf("K: %.4f, %.4f, %.4f\n %.4f, %.4f, %.4f\n", K[0][0], K[0][1], K[0][2], K[1][0], K[1][1], K[1][2]);

    ///< Innovation (residue): y = z - H * xₖ|ₖ₋₁
    float v_imu = ctrl_senfusion->v_imu_prev + a_imu * ctrl_senfusion->dt; ///< IMU velocity: v_imu = v_imu_prev + a_imu * dt
    float z[3][1] = {{p_enc}, {p_lidar}, {v_imu}}; ///< Measurement vector
    float y[3][1] = {0};
    matrixMultiplication(3, 2, 2, ctrl_senfusion->H, ctrl_senfusion->x_pred, y); // H * xₖ|ₖ₋₁
    matrixAddSub(3, 1, z, y, y, '-'); // z - H * xₖ|ₖ₋₁
    ctrl_senfusion->v_imu_prev = v_imu; ///< Update the IMU velocity for the next iteration
    // printf("y: %.4f, %.4f, %.4f\n", y[0][0], y[1][0], y[2][0]);

    ///< Update the state: xₖ = xₖ|ₖ₋₁ + K * y
    float x[2][1] = {0};
    matrixMultiplication(2, 3, 1, K, y, x); // K * y
    ctrl_senfusion->x[0][0] = ctrl_senfusion->x_pred[0][0] + x[0][0]; // xₖ = xₖ|ₖ₋₁ + K * y
    ctrl_senfusion->x[1][0] = ctrl_senfusion->x_pred[1][0] + x[1][0]; // xₖ = xₖ|ₖ₋₁ + K * y
    // printf("x: %.4f, %.4f\n", x[0][0], x[1][0]);

    ///< Update the error covariance matrix: Pₖ = (I - K * H) * Pₖ|ₖ₋₁
    float I_K_H[2][2] = {0};
    float KH[2][2] = {0};
    matrixMultiplication(2, 3, 2, K, ctrl_senfusion->H, KH); // K * H
    I_K_H[0][0] = 1 - KH[0][0]; // I - K * H
    I_K_H[0][1] = -KH[0][1];
    I_K_H[1][0] = -KH[1][0];
    I_K_H[1][1] = 1 - KH[1][1];
    matrixMultiplication(2, 2, 2, I_K_H, ctrl_senfusion->P_pred, ctrl_senfusion->P); // (I - K * H) * Pₖ|ₖ₋₁
    // printf("KH: %.4f, %.4f\n %.4f, %.4f\n", KH[0][0], KH[0][1], KH[1][0], KH[1][1]);
    // printf("P: %.4f, %.4f\n %.4f, %.4f\n", ctrl_senfusion->P[0][0], ctrl_senfusion->P[0][1], ctrl_senfusion->P[1][0], ctrl_senfusion->P[1][1]);
}

float ctrl_senfusion_calc_pid(ctrl_senfusion_t *ctrl_senfusion, float error)
{
    float output = 0;
    /* Add current error to the integral error */
    ctrl_senfusion->pid.integral_err += error;
    /* If the integral error is out of the range, it will be limited */
    ctrl_senfusion->pid.integral_err = MIN(ctrl_senfusion->pid.integral_err, ctrl_senfusion->pid.max_integral);
    ctrl_senfusion->pid.integral_err = MAX(ctrl_senfusion->pid.integral_err, ctrl_senfusion->pid.min_integral);

    /* Calculate the pid control value by location formula */
    /* u(k) = e(k)*Kp + (e(k)-e(k-1))*Kd + integral*Ki */
    output = error * ctrl_senfusion->pid.Kp +
             (error - ctrl_senfusion->pid.prev_err1) * ctrl_senfusion->pid.Kd +
             ctrl_senfusion->pid.integral_err * ctrl_senfusion->pid.Ki;

    /* If the output is out of the range, it will be limited */
    output = MIN(output, ctrl_senfusion->pid.max_output);
    output = MAX(output, ctrl_senfusion->pid.min_output);

    /* Update previous error */
    ctrl_senfusion->pid.prev_err1 = error;

    return output;
}

float ctrl_senfusion_calc_pid_z(ctrl_senfusion_t *ctrl_senfusion, float error)
{
    float control = -0.558*ctrl_senfusion->pid.prev_err2 - 0.254*ctrl_senfusion->pid.prev_err1 + 0.812*error
                  -ctrl_senfusion->pid.prev_u2 + 2*ctrl_senfusion->pid.prev_u1;

    ///< Update the PID controller state
    ctrl_senfusion->pid.prev_err2 = ctrl_senfusion->pid.prev_err1; // e(k-2) = e(k-1)
    ctrl_senfusion->pid.prev_err1 = error; // e(k-1) = e(k)
    ctrl_senfusion->pid.prev_u2 = ctrl_senfusion->pid.prev_u1; // u(k-2) = u(k-1)
    ctrl_senfusion->pid.prev_u1 = control; // u(k-1) = u(k)

    ///< Limit the control output
    control = MIN(control, ctrl_senfusion->pid.max_output);
    control = MAX(control, ctrl_senfusion->pid.min_output);

    return control; ///< Return the control output
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
