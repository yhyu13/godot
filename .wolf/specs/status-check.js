// Drift guard between the two sources of truth that track active work:
// `.wolf/specs-state.json` (the SDD work pointer) and `.wolf/STATUS.md` (the
// "read first" handoff doc). A fresh session reads STATUS.md first per the
// OpenWolf protocol, so it must mention the active spec id or the resume path
// breaks silently.
export function statusMentionsActiveSpec(statusMd, activeSpec) {
    return activeSpec.length > 0 && statusMd.includes(activeSpec);
}
//# sourceMappingURL=status-check.js.map