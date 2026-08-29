import { backupCorruptFile, readJsonFile, writeJsonAtomic } from "./persistence.js";
import { normalizeRecallPath } from "./cue-recall.js";
import { tokenizeClaim } from "./claims.js";
const STATUSES = ["active", "disputed", "superseded"];
function appendUnique(map, key, id) {
    if (!map[key])
        map[key] = [];
    if (!map[key].includes(id))
        map[key].push(id);
}
export function buildClaimIndex(claims) {
    const sorted = [...claims].sort((left, right) => right.updated_at.localeCompare(left.updated_at));
    const index = {
        version: 1,
        last_updated: new Date().toISOString(),
        claim_ids: sorted.map((claim) => claim.id),
        identity_index: {},
        token_index: {},
        path_index: {},
        status_index: {
            active: [],
            disputed: [],
            superseded: [],
        },
        evidence_event_index: {},
    };
    for (const claim of sorted) {
        index.identity_index[claim.identity_key] = claim.id;
        for (const token of tokenizeClaim(claim.statement)) {
            appendUnique(index.token_index, token, claim.id);
        }
        for (const filePath of claim.scope.paths ?? []) {
            appendUnique(index.path_index, normalizeRecallPath(filePath), claim.id);
        }
        index.status_index[claim.status].push(claim.id);
        for (const evidence of claim.evidence) {
            appendUnique(index.evidence_event_index, evidence.event_id, claim.id);
        }
    }
    return index;
}
function isRecord(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}
function isStringArray(value) {
    return Array.isArray(value) && value.every((item) => typeof item === "string");
}
function isStringMap(value) {
    return isRecord(value) && Object.values(value).every((item) => typeof item === "string");
}
function isStringArrayMap(value) {
    return isRecord(value) && Object.values(value).every(isStringArray);
}
function isStatusIndex(value) {
    return (isRecord(value) &&
        STATUSES.every((status) => isStringArray(value[status])));
}
export function isClaimIndex(value) {
    return (isRecord(value) &&
        value.version === 1 &&
        typeof value.last_updated === "string" &&
        Number.isFinite(Date.parse(value.last_updated)) &&
        isStringArray(value.claim_ids) &&
        isStringMap(value.identity_index) &&
        isStringArrayMap(value.token_index) &&
        isStringArrayMap(value.path_index) &&
        isStatusIndex(value.status_index) &&
        isStringArrayMap(value.evidence_event_index));
}
export function loadClaimIndex(indexPath, recoverCorrupt = false) {
    const parsed = readJsonFile(indexPath, false);
    if (parsed === null) {
        if (recoverCorrupt) {
            try {
                backupCorruptFile(indexPath);
            }
            catch { }
        }
        return null;
    }
    if (!isClaimIndex(parsed)) {
        if (recoverCorrupt)
            backupCorruptFile(indexPath);
        return null;
    }
    return parsed;
}
export function saveClaimIndex(indexPath, index) {
    index.last_updated = new Date().toISOString();
    writeJsonAtomic(indexPath, index);
}
function sameIds(left, right) {
    if (left.length !== right.length)
        return false;
    const expected = new Set(right);
    return new Set(left).size === expected.size && left.every((id) => expected.has(id));
}
function sameStringMap(left, right) {
    const leftKeys = Object.keys(left).sort();
    const rightKeys = Object.keys(right).sort();
    return (leftKeys.join("\0") === rightKeys.join("\0") &&
        leftKeys.every((key) => left[key] === right[key]));
}
function sameArrayMap(left, right) {
    const leftKeys = Object.keys(left).sort();
    const rightKeys = Object.keys(right).sort();
    return (leftKeys.join("\0") === rightKeys.join("\0") &&
        leftKeys.every((key) => sameIds(left[key], right[key])));
}
export function claimIndexNeedsRebuild(index, claims) {
    if (!index)
        return true;
    const expected = buildClaimIndex(claims);
    return !(sameIds(index.claim_ids, expected.claim_ids) &&
        sameStringMap(index.identity_index, expected.identity_index) &&
        sameArrayMap(index.token_index, expected.token_index) &&
        sameArrayMap(index.path_index, expected.path_index) &&
        sameArrayMap(index.status_index, expected.status_index) &&
        sameArrayMap(index.evidence_event_index, expected.evidence_event_index));
}
//# sourceMappingURL=claim-index.js.map