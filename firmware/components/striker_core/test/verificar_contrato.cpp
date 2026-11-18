// Verificación del contrato de Striker v3 (Entrega 1).
//
// No es firmware: es una unidad de compilación que demuestra que
//  1. todas las interfaces se pueden implementar y usar sin memoria dinámica,
//     sin excepciones y sin RTTI;
//  2. los datos que viajan por colas de FreeRTOS o se guardan en NVS son
//     trivialmente copiables;
//  3. la máquina de estados cumple las transiciones de seguridad del diseño.
// Lo compila docs/striker_v3/herramientas/verificar_interfaces.sh con el g++
// del PC y con el compilador del ESP32-S3, y revisa que el objeto no llame a
// malloc, a operator new ni al soporte de excepciones.

#include <type_traits>

#include "striker/striker.hpp"

using namespace striker;

// --- 2. Datos copiables por colas y NVS -------------------------------------
static_assert(std::is_trivially_copyable_v<EncoderReading>);
static_assert(std::is_trivially_copyable_v<ImuSample>);
static_assert(std::is_trivially_copyable_v<WheelSpeeds>);
static_assert(std::is_trivially_copyable_v<BodyTwist>);
static_assert(std::is_trivially_copyable_v<Pose2D>);
static_assert(std::is_trivially_copyable_v<TrajectoryPoint>);
static_assert(std::is_trivially_copyable_v<MotorCommand>);
static_assert(std::is_trivially_copyable_v<WheelGains>);
static_assert(std::is_trivially_copyable_v<AutotuneSample>);
static_assert(std::is_trivially_copyable_v<RouteSpec>);
static_assert(std::is_trivially_copyable_v<BatteryStatus>);
static_assert(std::is_trivially_copyable_v<RobotConfig>);
static_assert(std::is_trivially_copyable_v<EstimatorInput>);
static_assert(std::is_trivially_copyable_v<EstimatorOutput>);

// Las interfaces no se copian ni se destruyen desde afuera.
static_assert(!std::is_copy_constructible_v<ISensorIMU>);
static_assert(!std::is_copy_assignable_v<IMotors>);
static_assert(!std::is_destructible_v<IController>);

// --- 3. Máquina de estados ---------------------------------------------------
namespace {
constexpr Guards kAllOk{true, true, true, true, true};
constexpr Guards kNoLink{true, true, false, true, true};
constexpr Guards kLowBattery{false, true, true, true, true};
}  // namespace

// Al arrancar no se mueve nada: Boot solo sale con InitOk o InitFailed.
static_assert(next(Mode::Boot, Event::CmdTeleop, kAllOk) == Mode::Boot);
static_assert(next(Mode::Boot, Event::InitOk, kAllOk) == Mode::Idle);
static_assert(!motorsArmed(Mode::Boot) && !motorsArmed(Mode::Idle) && !motorsArmed(Mode::Fault));
// Moverse exige batería, sensores y enlace con la app web.
static_assert(next(Mode::Idle, Event::CmdTeleop, kAllOk) == Mode::Teleop);
static_assert(next(Mode::Idle, Event::CmdTeleop, kNoLink) == Mode::Idle);
static_assert(next(Mode::Idle, Event::CmdRoute, kLowBattery) == Mode::Idle);
// Perder el enlace detiene cualquier movimiento.
static_assert(next(Mode::Teleop, Event::LinkTimeout, kAllOk) == Mode::Idle);
static_assert(next(Mode::Route, Event::LinkTimeout, kAllOk) == Mode::Idle);
static_assert(next(Mode::Autotune, Event::LinkTimeout, kAllOk) == Mode::Idle);
// Cualquier falla lleva a Fallo, desde cualquier modo.
static_assert(next(Mode::Route, Event::BatteryCritical, kAllOk) == Mode::Fault);
static_assert(next(Mode::Teleop, Event::UlpLost, kAllOk) == Mode::Fault);
static_assert(next(Mode::Idle, Event::ControlOverrun, kAllOk) == Mode::Fault);
// De Fallo solo se sale por orden del operador y con la causa resuelta.
static_assert(next(Mode::Fault, Event::CmdTeleop, kAllOk) == Mode::Fault);
static_assert(next(Mode::Fault, Event::CmdResetFault, Guards{true, true, true, true, false}) == Mode::Fault);
static_assert(next(Mode::Fault, Event::CmdResetFault, kAllOk) == Mode::Idle);

// --- 1. Implementaciones mínimas, todas estáticas ----------------------------
namespace {

class FakeImu final : public ISensorIMU {
public:
    Status init() noexcept override { return Status::Ok; }
    Status read(ImuSample& out) noexcept override
    {
        out = sample_;
        return Status::Ok;
    }
    const char* name() const noexcept override { return "fake-imu"; }

private:
    ImuSample sample_{};
};

class FakeEncoders final : public IWheelEncoders {
public:
    Status init() noexcept override { return Status::Ok; }
    Status read(EncoderReading& out) noexcept override
    {
        out.round = ++round_;
        return Status::Ok;
    }

private:
    std::uint32_t round_{};
};

class FakeMotors final : public IMotors {
public:
    Status init() noexcept override { return Status::Ok; }
    Status arm() noexcept override
    {
        armed_ = true;
        return Status::Ok;
    }
    void disarm() noexcept override { armed_ = false; }
    bool armed() const noexcept override { return armed_; }
    Status set(const MotorCommand& cmd) noexcept override
    {
        if (!armed_) {
            return Status::NotReady;
        }
        last_ = cmd;
        return Status::Ok;
    }
    void stopAll() noexcept override { last_ = MotorCommand{}; }

private:
    bool armed_{};
    MotorCommand last_{};
};

class FakePid final : public IController {
public:
    Status configure(float, float lo, float hi) noexcept override
    {
        lo_ = lo;
        hi_ = hi;
        return Status::Ok;
    }
    float update(float sp, float meas) noexcept override
    {
        float u = g_.kp * (sp - meas);
        return u < lo_ ? lo_ : (u > hi_ ? hi_ : u);
    }
    void reset() noexcept override {}
    Status setGains(const PidGains& g) noexcept override
    {
        g_ = g;
        return Status::Ok;
    }

private:
    PidGains g_{1.0f, 0.0f, 0.0f};
    float lo_{-80.0f};
    float hi_{80.0f};
};

class FakeKinematics final : public IKinematics {
public:
    void toWheels(const BodyTwist& b, WheelSpeeds& w) const noexcept override
    {
        for (auto& v : w.rad_s) {
            v = b.vx_m_s;
        }
    }
    void toBody(const WheelSpeeds& w, BodyTwist& b) const noexcept override { b.vx_m_s = w.rad_s[0]; }
};

class FakeEstimator final : public IStateEstimator {
public:
    void reset(const Pose2D& p) noexcept override { out_.pose = p; }
    Status step(const EstimatorInput&, EstimatorOutput& out) noexcept override
    {
        out = out_;
        return Status::Ok;
    }
    const char* name() const noexcept override { return "fake-ekf"; }

private:
    EstimatorOutput out_{};
};

FakeImu g_imu;
FakeEncoders g_enc;
FakeMotors g_motors;
FakePid g_pid[kNumWheels];
FakeKinematics g_kin;
FakeEstimator g_est;

}  // namespace

// Un ciclo del lazo de ruedas escrito solo contra interfaces: así lo usará T_ctrl.
extern "C" int striker_contract_ctrl_step(void)
{
    IWheelEncoders& enc = g_enc;
    IMotors& motors = g_motors;
    EncoderReading r{};
    if (enc.read(r) != Status::Ok) {
        motors.stopAll();
        return -1;
    }
    MotorCommand cmd{};
    for (std::size_t i = 0; i < kNumWheels; ++i) {
        IController& pid = g_pid[i];
        cmd.duty_pct[i] = pid.update(1.0f, r.angle_rad[i]);
    }
    return motors.set(cmd) == Status::Ok ? 0 : 1;
}

// Un paso de navegación, también solo contra interfaces (T_nav).
extern "C" int striker_contract_nav_step(void)
{
    ISensorIMU& imu = g_imu;
    IKinematics& kin = g_kin;
    IStateEstimator& est = g_est;
    EstimatorInput in{};
    in.dt_s = 0.01f;
    in.imu_valid = imu.read(in.imu) == Status::Ok;
    WheelSpeeds w{};
    kin.toBody(w, in.odometry);
    EstimatorOutput out{};
    return est.step(in, out) == Status::Ok ? 0 : 1;
}
