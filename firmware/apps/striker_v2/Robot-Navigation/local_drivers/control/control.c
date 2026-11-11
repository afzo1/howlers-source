#include "control.h"

void control_init(control_t *ctrl, float dt, pid_block_t pid)
{
    
    ///< ---------------- PID CONTROLLER -----------------
    ctrl->pid = pid; ///< Set the PID controller
    ctrl->pid.prev_err1 = 0; ///< Initialize the previous error
    ctrl->pid.prev_err2 = 0; ///< Initialize the previous error
    ctrl->pid.prev_u1 = 0; ///< Initialize the previous control output
    ctrl->pid.prev_u2 = 0; ///< Initialize the previous control output
    ctrl->pid.integral_err = 0; ///< Initialize the integral error
}


float control_calc_pid(control_t *ctrl, float value)
{
    
    float error = ctrl->setpoint - value;

    float output = 0;
    /* Add current error to the integral error */
    ctrl->pid.integral_err += error;
    /* If the integral error is out of the range, it will be limited */
    ctrl->pid.integral_err = MIN(ctrl->pid.integral_err, ctrl->pid.max_integral);
    ctrl->pid.integral_err = MAX(ctrl->pid.integral_err, ctrl->pid.min_integral);

    /* Calculate the pid control value by location formula */
    /* u(k) = e(k)*Kp + (e(k)-e(k-1))*Kd + integral*Ki */
    output = error * ctrl->pid.Kp +
             (error - ctrl->pid.prev_err1) * ctrl->pid.Kd +
             ctrl->pid.integral_err * ctrl->pid.Ki;

    /* If the output is out of the range, it will be limited */
    output = MIN(output, ctrl->pid.max_output);
    output = MAX(output, ctrl->pid.min_output);

    /* Update previous error */
    ctrl->pid.prev_err1 = error;

    return output;
}

float control_calc_pid_z(control_t *ctrl, float value)
{
    float error = ctrl->setpoint - value;
    float control = 0.0f;

    if (ctrl->pid.order == 2)
    {
        control = (ctrl->pid.a0 * ctrl->pid.prev_err2 +
                   ctrl->pid.a1 * ctrl->pid.prev_err1 +
                   ctrl->pid.a2 * error
                   - ctrl->pid.b0 * ctrl->pid.prev_u2
                   - ctrl->pid.b1 * ctrl->pid.prev_u1) / ctrl->pid.b2;

        // Update history
        ctrl->pid.prev_err2 = ctrl->pid.prev_err1;
        ctrl->pid.prev_err1 = error;
        ctrl->pid.prev_u2 = ctrl->pid.prev_u1;
        ctrl->pid.prev_u1 = control;
    }
    else if (ctrl->pid.order == 1)
    {
        control = (ctrl->pid.a1 * error +
                   ctrl->pid.a0 * ctrl->pid.prev_err1
                   - ctrl->pid.b0 * ctrl->pid.prev_u1) / ctrl->pid.b1;

        // Update history
        ctrl->pid.prev_err1 = error;
        ctrl->pid.prev_u1 = control;
    }

    // Clamp control output
    control = MIN(control, ctrl->pid.max_output);
    control = MAX(control, ctrl->pid.min_output);

    return control;
}

