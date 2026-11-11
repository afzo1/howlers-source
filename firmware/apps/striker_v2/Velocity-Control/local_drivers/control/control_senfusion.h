/**
 * \file        control_senfusion.h
 * \brief       Control and Sensor Fusion library
 * \details
 * 
 * 
 * \author      MaverickST
 * \version     0.0.4
 * \date        16/05/2025
 * \copyright   Unlicensed
 */

#ifndef __CONTROL_SENFUSION_H__
#define __CONTROL_SENFUSION_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b)) ///< Macro to get the minimum value
#define MAX(a,b) ((a)>(b)?(a):(b)) ///< Macro to get the maximum value

typedef struct {
    float Kp; // PID Kp value
    float Ki; // PID Ki value
    float Kd; // PID Kd value
    float prev_err1; // e(k-1)
    float prev_err2; // e(k-2)
    float prev_u1; // u(k-1)
    float prev_u2; // u(k-2)
    float integral_err;  // Sum of error
    float last_output;  // PID output in last control period
    float max_output;   // PID maximum output limitation
    float min_output;   // PID minimum output limitation
    float max_integral; // PID maximum integral value limitation
    float min_integral; // PID minimum integral value limitation
} pid_block_t; ///< PID controller block

typedef struct
{
    float x_pred[2][1]; ///< Predicted state vector
    float P_pred[2][2]; ///< Predicted error covariance matrix
    float x[2][1]; ///< State vector

    float a_imu; ///< IMU acceleration
    float v_imu_prev; ///< IMU velocity
    float p_enc; ///< Encoder position
    float p_lidar; ///< Lidar position

    float p_ref[2]; ///< Reference position
    uint32_t k; ///< Sample time
    float dt; ///< Time step

    ///< Kalman filter Variables
    float P[2][2]; ///< Error covariance matrix
    float Q[2][2]; ///< Process noise covariance matrix
    float R[3][3]; ///< Measurement noise covariance matrix

    ///< Process model
    /**
     * @brief Process model: x(k+1) = A*x(k) + B*u(k)
     * State vector: x = [x, y]
    */
    float u; ///< Control input
    float A[2][2]; ///< Process model matrix
    float B[2][1]; ///< Control input matrix
    float C[2][2]; ///< Process model matrix

    ///< Measurement model
    /**
     * @brief Measurement model: z(k) = H*x(k) + v(k)
     * Measurement vector: z = [p_enc, p_lidar, v_imu]
    */
    float z[3][1]; ///< Measurement vector
    float H[3][2]; ///< Measurement matrix
    float H_T[2][3]; ///< Transpose of the measurement matrix

    /**
     * @brief PID controller parameters
     * 
     */
    pid_block_t pid; ///< PID controller block

} ctrl_senfusion_t;


/**
 * @brief Initialize the control and sensor fusion structure
 * 
 * @param ctrl_senfusion Pointer to the control and sensor fusion structure
 */
void ctrl_senfusion_init(ctrl_senfusion_t *ctrl_senfusion, pid_block_t pid, float dt);

/**
 * @brief Predict the next state of the system
 * 
 * @param ctrl_senfusion Pointer to the control and sensor fusion structure
 * @param u Control input
 */
void ctrl_senfusion_predict(ctrl_senfusion_t *ctrl_senfusion, float u);

/**
 * @brief Update the control and sensor fusion structure
 * 
 * @param ctrl_senfusion Pointer to the control and sensor fusion structure
 */
void ctrl_senfusion_update(ctrl_senfusion_t *ctrl_senfusion, float p_enc, float p_lidar, float a_imu, uint32_t k);

/**
 * @brief Get the position from the sensor fusion
 * 
 * @param ctrl_senfusion 
 * @return float 
 */
static inline float ctrl_senfusion_get_pos(ctrl_senfusion_t *ctrl_senfusion)
{
    return ctrl_senfusion->x[0][0]; ///< Get the position from the sensor fusion
}

/**
 * @brief Get the velocity from the sensor fusion
 * 
 * @param ctrl_senfusion 
 * @return float 
 */
static inline float ctrl_senfusion_get_vel(ctrl_senfusion_t *ctrl_senfusion)
{
    return ctrl_senfusion->x[1][0]; ///< Get the velocity from the sensor fusion
}

/**
 * @brief Calculate the PID control value
 * 
 * @param ctrl_senfusion Pointer to the control and sensor fusion structure
 * @param error Error value
 * @return float PID control value
 */
float ctrl_senfusion_calc_pid(ctrl_senfusion_t *ctrl_senfusion, float error);

/**
 * @brief Calculate the PID discrete control value
 * 
 * @param ctrl_senfusion 
 * @param error 
 * @return float 
 */
float ctrl_senfusion_calc_pid_z(ctrl_senfusion_t *ctrl_senfusion, float error);

///<-------------------------------------------------------------
///<--------------- LINEAR ALGEBRA FUNCTIONS --------------------
///<-------------------------------------------------------------


/**
 * @brief Matrix multiplication
 * 
 * A[rowsA][colsA] * B[colsA][colsB] = C[rowsA][colsB]
 * 
 */
void matrixMultiplication(int rowsA, int colsA, int colsB, float A[rowsA][colsA], float B[colsA][colsB], float C[rowsA][colsB]);

/**
 * @brief Matrix addition and subtraction
 * 
 * A[rows][cols] + B[rows][cols] = C[rows][cols]
 * 
 */
void matrixAddSub(int rows, int cols, float A[rows][cols], float B[rows][cols], float C[rows][cols], char operation);

/**
 * @brief Matrix transpose
 * 
 * A[rows][cols] = B[cols][rows]
 * 
 */
void matrixTranspose(int rows, int cols, float A[rows][cols], float B[cols][rows]);

/// <-------------------------------------------------------------
/// <------------------ LU DECOMPOSITION -------------------------
/// <-------------------------------------------------------------

#define N 3       // Size of matrix A (NxN)
#define M 2       // Number of columns in B (right-hand sides)

void LU_decomposition(float A[N][N], float L[N][N], float U[N][N]);

void forward_substitution(float L[N][N], float b[N], float y[N]);

void backward_substitution(float U[N][N], float y[N], float x[N]);


/**
 * @brief This function solves the linear system Ax = B using LU decomposition.
 * It will be used to computes the Kalman Gain using LU decomposition.
 * 
 * This function avoids directly inverting the innovation covariance matrix S.
 * Instead, it uses LU decomposition to solve a linear system.
 * 
 * Solve X = B * inv(A) using LU decomposition without explicit inversion.
 * 
 * Given a square matrix A (N×N) and a matrix B (M×N), this routine computes
 * X = B * inv(A) by solving each row of B as a right‐hand‐side system:
 *      X[i,:] * A = B[i,:]
 * which is reformulated as
 *      (A^T) * z = b
 * for z = X[i,:]^T, b = B[i,:]^T.
 *
 * @param  A    Input matrix A of size N×N.
 * @param  B    Input matrix B of size M×N.
 * @param  X    Output matrix X of size M×N, containing B * inv(A).
 * 
 * This approach is numerically stable and efficient compared to computing inv(S).
 */
void solve_LU_system(float A[N][N], float B[M][N], float K[M][N]);

#endif // __CONTROL_SENFUSION_H__
