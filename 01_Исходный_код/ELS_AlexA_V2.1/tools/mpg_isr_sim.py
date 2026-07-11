#!/usr/bin/env python3
"""
Симуляция логики MPG Z + jog cruise + ISR stepper (упрощённая модель stepper_gen).
Проверяет инварианты, которые могут дать краш/UB на AVR/Wokwi.
"""
from dataclasses import dataclass, field
from enum import IntEnum

STEP_ISR_FREQ_HZ = 1000000 // 33
MPG_LOOKAHEAD = 4
MPG_MAX_RUNWAY_STEPS = 512
MPG_MAX_CMD_AHEAD = 4096
ACCEL_MM_S2_SCALE = 50.0

# Z defaults (config_machine.h)
MOTOR = 200
MICRO = 2
PITCH_MM = 1.0
FEED_ACCEL = 3
SPM = MOTOR * MICRO / PITCH_MM  # 400 steps/mm
NOMINAL_MM_MIN = 160.0


class TR(IntEnum):
    MPG_COMMIT = 31
    PLANNER_EXEC = 40
    JOG_STOP = 41
    DDS_START = 50
    RETARGET = 51
    JOG_RELEASE = 52
    ISR_ENTER = 60
    ISR_AFTER_X = 61
    ISR_AFTER_Z = 62


def mm_min_to_sps(mm_min: float) -> int:
    mm_min = max(mm_min, 1.0)
    return max(int(mm_min * SPM / 60.0), 1)


def dds_calc_increment(sps: int) -> int:
    sps = max(sps, 1)
    return int((sps << 31) / STEP_ISR_FREQ_HZ)


def axis_accel_steps_s2() -> int:
    a = int(FEED_ACCEL * ACCEL_MM_S2_SCALE * SPM + 0.5)
    return max(a, 1)


def accel_distance(rate_hi: int, rate_lo: int, accel: int) -> int:
    if accel == 0 or rate_hi <= rate_lo:
        return 0
    diff = rate_hi * rate_hi - rate_lo * rate_lo
    return diff // (2 * accel)


@dataclass
class Axis:
    accumulator: int = 0
    step_increment: int = 0
    position: int = 0
    target: int = 0
    direction: int = 1
    enabled: int = 0


@dataclass
class MotionProf:
    active: int = 0
    jog_cruise: int = 0
    bl_drain: int = 0
    master_axis: int = 1  # Z
    step_events: int = 0
    step_count: int = 0xFFFFFFFF
    accelerate_until: int = 0
    decelerate_after: int = 0xFFFFFFFE
    current_rate: int = 1
    nominal_rate: int = 1
    initial_rate: int = 1
    final_rate: int = 1
    acceleration: int = 1
    axis_steps: list = field(default_factory=lambda: [0, 0])


class Sim:
    def __init__(self):
        self.ax = [Axis(), Axis()]
        self.prof = MotionProf()
        self.mpg_cmd = [0, 0]
        self.trace_main = 0
        self.trace_isr = 0
        self.isr_ticks = 0
        self.violations: list[str] = []

    def chk(self, cond: bool, msg: str):
        if not cond:
            self.violations.append(msg)

    def motion_profile_denom(self) -> int:
        p = self.prof
        if p.jog_cruise:
            d = max(p.axis_steps[0], p.axis_steps[1])
            return max(d, 1)
        return max(p.step_count, 1)

    def motion_apply_rates(self):
        p = self.prof
        r = p.current_rate
        denom = self.motion_profile_denom()
        self.chk(denom > 0, f"denom=0 active={p.active} steps={p.axis_steps}")
        for axis in (0, 1):
            if p.axis_steps[axis] > 0:
                sz = (r * p.axis_steps[axis]) // denom
                sz = max(sz, 1)
                self.ax[axis].step_increment = dds_calc_increment(sz)
            else:
                self.ax[axis].step_increment = 0

    def motion_profile_step(self, axis: int):
        p = self.prof
        if not p.active or axis != p.master_axis:
            return
        if p.current_rate < 1:
            p.current_rate = 1
        p.step_events += 1
        if p.step_events <= p.accelerate_until:
            inc = (p.acceleration << 14) // p.current_rate
            inc = max(inc, 1)
            old = p.current_rate
            p.current_rate += inc
            if p.current_rate < old:  # overflow uint32
                self.violations.append(f"current_rate overflow wrap at step_events={p.step_events}")
            if p.current_rate > p.nominal_rate:
                p.current_rate = p.nominal_rate
        self.motion_apply_rates()

    def dds_axis_step(self, axis: int):
        a = self.ax[axis]
        if not a.enabled or a.step_increment == 0:
            return
        p = self.prof
        if p.jog_cruise:
            if a.position == a.target:
                return
            fwd = 1 if a.target > a.position else 0
            a.direction = fwd
        elif a.position == a.target:
            return
        else:
            fwd = 1 if a.target > a.position else 0
            a.direction = fwd

        a.accumulator += a.step_increment
        if a.accumulator >= 0x80000000:
            a.accumulator -= 0x80000000
            a.position += 1 if fwd else -1
            if p.active:
                self.motion_profile_step(axis)

    def stepper_generate_steps(self):
        self.trace_isr = TR.ISR_ENTER
        self.dds_axis_step(0)
        self.trace_isr = TR.ISR_AFTER_X
        self.dds_axis_step(1)
        self.trace_isr = TR.ISR_AFTER_Z
        self.isr_ticks += 1

    def dds_motion_start_jog_cruise(self, tz: int, runway: int):
        self.trace_main = TR.DDS_START
        p = self.prof
        p.active = 0
        z = self.ax[1]
        z.target = tz
        nominal = mm_min_to_sps(NOMINAL_MM_MIN)
        entry = 1
        accel = axis_accel_steps_s2()
        p.jog_cruise = 1
        p.axis_steps[0] = 0
        p.axis_steps[1] = runway
        p.master_axis = 1
        p.acceleration = accel
        p.nominal_rate = nominal
        p.initial_rate = entry
        p.current_rate = entry
        p.final_rate = 1
        p.step_events = 0
        p.step_count = 0xFFFFFFFF
        p.decelerate_after = 0xFFFFFFFE
        ad = max(accel_distance(nominal, entry, accel), 1)
        p.accelerate_until = ad
        p.active = 1
        z.enabled = 1
        self.motion_apply_rates()
        # sanity shift overflow check
        shift = p.acceleration << 14
        if p.acceleration > 0 and shift // p.acceleration != (1 << 14):
            self.violations.append(f"accel shift overflow: a={p.acceleration}")

    def dds_motion_jog_retarget(self, tz: int, runway: int) -> bool:
        self.trace_main = TR.RETARGET
        if not self.prof.active or not self.prof.jog_cruise:
            return False
        if runway == 0:
            self.prof.active = 0
            self.ax[1].enabled = 0
            return True
        self.ax[1].target = tz
        self.prof.axis_steps[1] = runway
        self.prof.master_axis = 1
        self.motion_apply_rates()
        return True

    def mpg_planner_commit(self, cmd_z: int, pos_z: int, cruise_active: bool):
        self.trace_main = TR.MPG_COMMIT
        runway = min(MPG_LOOKAHEAD, MPG_MAX_RUNWAY_STEPS)
        err = cmd_z - pos_z
        sign = 1 if err > 0 else (-1 if err < 0 else 0)
        cmd_tgt = cmd_z
        if not cruise_active and sign:
            if sign > 0 and err < runway:
                cmd_tgt = min(pos_z + runway, cmd_z)
            elif sign < 0 and (-err) < runway:
                cmd_tgt = max(pos_z - runway, cmd_z)
        tz = cmd_tgt
        self.trace_main = TR.PLANNER_EXEC
        if cruise_active:
            self.dds_motion_jog_retarget(tz, abs(tz - pos_z) or runway)
        else:
            self.dds_motion_start_jog_cruise(tz, abs(tz - pos_z) or runway)

    def run_mpg_ticks(self, n_ticks: int, isr_per_tick: int = 8000):
        """Каждый тик MPG + прогон ISR между тиками."""
        for tick in range(1, n_ticks + 1):
            self.mpg_cmd[1] = tick
            pos = self.ax[1].position
            cruise = self.prof.active and self.prof.jog_cruise
            self.mpg_planner_commit(self.mpg_cmd[1], pos, cruise)
            for _ in range(isr_per_tick):
                self.stepper_generate_steps()
                if self.ax[1].position == self.ax[1].target and self.prof.step_events > self.prof.accelerate_until:
                    break
            # idle stop simulation
            self.trace_main = TR.JOG_STOP
            self.trace_main = TR.JOG_RELEASE
            self.prof.active = 0
            self.prof.jog_cruise = 0
            self.ax[1].enabled = 0
            self.mpg_cmd[1] = self.ax[1].position


def main():
    print("=== MPG/ISR logic simulation (Z axis, cruise) ===")
    print(f"SPM={SPM}, nominal_sps={mm_min_to_sps(NOMINAL_MM_MIN)}, accel_s2={axis_accel_steps_s2()}")
    sim = Sim()
    sim.run_mpg_ticks(20, isr_per_tick=12000)
    print(f"ISR ticks total: {sim.isr_ticks}")
    print(f"Final Z pos={sim.ax[1].position} target={sim.ax[1].target}")
    print(f"Last main trace={sim.trace_main} ISR trace={sim.trace_isr}")
    if sim.violations:
        print("VIOLATIONS:")
        for v in sim.violations:
            print(" ", v)
    else:
        print("No invariant violations in simplified model.")

    # nested ISR stack estimate (bytes, rough)
    print("\n=== Stack nesting estimate (Wokwi) ===")
    est = {
        "main→motion_jog_poll→DBG Serial": 180,
        "TIMER5 ui_lcd_update→LiquidCrystal::print": 120,
        "TIMER1 stepper_generate_steps chain": 220,
    }
    total_nested = est["TIMER5 ui_lcd_update→LiquidCrystal::print"] + est["TIMER1 stepper_generate_steps chain"]
    print(f"  main+Serial ~{est['main→motion_jog_poll→DBG Serial']} B")
    print(f"  Timer5 LCD ISR ~{est['TIMER5 ui_lcd_update→LiquidCrystal::print']} B")
    print(f"  Timer1 nested in Timer5 ~{est['TIMER1 stepper_generate_steps chain']} B")
    print(f"  Worst case nested ~{total_nested} B + main frame")
    print("  AVR stack headroom ~3670 B — nested ISR under LCD is primary crash candidate")


if __name__ == "__main__":
    main()
