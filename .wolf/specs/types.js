// Spec-driven development (SDD) state model for OpenWolf.
// One durable row per project: which spec is active, what phase it is in,
// which numbered task is current, and whether the work is active/paused/
// blocked/complete. "complete" lives only in `status` — the terminal state —
// so it never collides with the phase axis.
export const SPEC_PHASES = ["specify", "plan", "tasks", "implement"];
export const SPEC_STATUSES = ["active", "paused", "blocked", "complete"];
export function createEmptySpecState(now) {
    return {
        version: 1,
        activeSpec: null,
        phase: "specify",
        currentTask: null,
        status: "active",
        updatedAt: now ?? new Date().toISOString(),
    };
}
export function isSpecPhase(v) {
    return typeof v === "string" && SPEC_PHASES.includes(v);
}
export function isSpecStatus(v) {
    return typeof v === "string" && SPEC_STATUSES.includes(v);
}
//# sourceMappingURL=types.js.map