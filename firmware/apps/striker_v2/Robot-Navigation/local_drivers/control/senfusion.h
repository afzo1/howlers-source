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

#ifndef __SENFUSION_H__
#define __SENFUSION_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b)) ///< Macro to get the minimum value
#define MAX(a,b) ((a)>(b)?(a):(b)) ///< Macro to get the maximum value

/**
 * Configuration struct for initializing the sensor fusion filter.
 * This filter estimates a single variable using two noisy measurements.
 */
typedef struct {
    float A;    ///< State transition coefficient (e.g. 1.0)
    float B;    ///< Input coefficient (e.g. dt if input is a·dt, or 0)
    float Q;    ///< Process noise variance
    float R1;   ///< Measurement noise variance of sensor 1 (wheel)
    float R2;   ///< Measurement noise variance of sensor 2 (IMU)
} senfusion_config_t;

/**
 * Internal state of the sensor fusion filter.
 */
typedef struct {
    float x;    ///< Estimated state
    float P;    ///< Estimate variance
    float A, B; ///< Model coefficients
    float Q;    ///< Process noise variance
    float R1;   ///< Var(sensor1)
    float R2;   ///< Var(sensor2)
} senfusion_t;

/**
 * @brief   Initialize the 1D fusion filter.
 * @param   f   Pointer to filter state
 * @param   cfg Pointer to configuration
 */
void senfusion_init(senfusion_t *f, const senfusion_config_t *cfg);

/**
 * @brief   Predict step: propagate state with control/input.
 * @param   f   Pointer to filter state
 * @param   u   Control input (e.g. a·dt, or 0 if unused)
 */
void senfusion_predict(senfusion_t *f, float u);

/**
 * @brief   Sequentially fuse two scalar measurements into one state.
 * @param   f   Pointer to filter state
 * @param   z1  First measurement (e.g. wheel velocity)
 */
void senfusion_update1(senfusion_t *f, float z1);

/**
 * @brief   Sequentially fuse two scalar measurements into one state.
 * @param   f   Pointer to filter state
 * @param   z2  Second measurement (e.g. IMU-derived velocity)
 */
void senfusion_update2(senfusion_t *f, float z2);

/**
 * @brief Get the current estimated value from the sensor fusion filter.
 * 
 * @param senfusion 
 * @return float 
 */
static inline float senfusion_get_state(senfusion_t *senfusion)
{
    return senfusion->x;
}

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

// -------------------------------------------------------------
// ---------------------- KALMAN FILTER 1D ---------------------
// -------------------------------------------------------------

/**
 * @brief Kalman filter 1D structure
 * 
 */
typedef struct {
    float x;  // estimated value
    float P;  // estimation error covariance
    float Q;  // process noise
    float R;  // measurement noise
} kalman1D_t;

/**
 * @brief Initialize the Kalman filter 1D structure
 * 
 * @param kf Pointer to the Kalman filter 1D structure
 * @param Q Process noise covariance
 * @param R Measurement noise covariance
 */
void kalman1D_init(kalman1D_t *kf, float Q, float R);

/**
 * @brief Update the Kalman filter 1D structure with a new measurement
 * 
 * @param kf Pointer to the Kalman filter 1D structure
 * @param meas Measurement value
 * @return float Estimated value
 */
float kalman1D_update(kalman1D_t *kf, float meas);

///< -------------------------------------------------------------
///< ------- ROBOT MODEL TRANSFORMATIONS AND FUNTIONS ------------
///< -------------------------------------------------------------
#include <time.h>

/**
 * This module provides functions to convert between body-frame velocities
 * (vbx, vby, wb) and individual wheel angular velocities (w1, w2, w3),
 * along with a test routine to validate the forward kinematics inversion.
 *
 * Coordinate conventions:
 *  - vbx: linear velocity along the robot's x-axis (forward)
 *  - vby: linear velocity along the robot's y-axis (sideways)
 *  - wb:  angular velocity about the robot's center (positive CCW)
 *  - wi:  angular velocity of wheel i (positive CW rotation)
 */

#define WHEEL_RADIUS 0.03f ///< Wheel radius in meters
#define ROBOT_RADIUS 0.16f ///< Robot diameter in meters
#define DELTA 0.523598  ///< Robot delta angle in radians (1.047198 for 60 degrees) (0.523598 radians for 30 degrees)
#define TOLERANCE 1e-5f

/**
 * @brief Given the velocity body velocity (vbx, vby) and the wheel base (wb), 
 * this function calculates the inverse kinematics to update the control and sensor fusion structure.
 * 
 * [ w1 ]       [ -sin(delta)  -cos(delta)   d ]     [ v_bx     ]
 * [ w2 ] = n/r [     1            0         d ]  *  [ v_by     ]
 * [ w3 ]       [ -sin(delta)   cos(delta)   d ]     [ omega_b  ]
 * 
 * @param vbx 
 * @param vby 
 * @param wb 
 * @param w1 Pointer to the first wheel speed
 * @param w2 Pointer to the second wheel speed
 * @param w3 Pointer to the third wheel speed
 */
void calc_invkinematics(float vbx, float vby, float wb, float *w1, float *w2, float *w3);

/**
 * @brief Compute forward kinematics: wheel speeds to body velocities.
 *
 * This function inverts the inverse-kinematics mapping to recover the robot's
 * body-frame linear and angular velocities from the three wheel angular speeds.
 *
 * The derivation uses the inverse of the 3x3 matrix relating [vbx, vby, wb]^T
 * to [w1, w2, w3]^T, scaled by the wheel radius.
 *
 * Mathematical expressions:
 *   vbx = r * (-w1 + 2*w2 - w3) / (2*(1 + sin(δ)))
 *   vby = r * (-w1 + w3) / (2*cos(δ))
 *   wb  = r * (w1 + 2*sin(δ)*w2 + w3) / (2*R*(1 + sin(δ)))
 *
 * @param w1    Angular velocity of wheel 1.
 * @param w2    Angular velocity of wheel 2.
 * @param w3    Angular velocity of wheel 3.
 * @param vbx   Pointer to store resulting linear velocity in x-direction.
 * @param vby   Pointer to store resulting linear velocity in y-direction.
 * @param wb    Pointer to store resulting angular velocity about robot center.
 */
void calc_fordwkinematics(float w1, float w2, float w3, float *vbx, float *vby, float *wb);

/**
 * @brief Test forward kinematics by round-trip inversion.
 *
 * Generates random body velocities, computes wheel speeds via inverse kinematics,
 * then recovers body velocities via forward kinematics and verifies accuracy.
 *
 * @param num_tests Number of random trials to perform.
 */
void test_forward_kinematics(int num_tests);

#endif // __ENFUSION_H__
