import * as crypto from "node:crypto";
import { validateClaimObservation } from "./claims.js";
import { backupCorruptFile, readJsonFile, writeJsonAtomic } from "./persistence.js";
const CANDIDATE_STATUSES = new Set([
    "pending",
    "approved",
    "rejected",
]);
function isRecord(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}
function isTimestamp(value) {
    return typeof value === "string" && Number.isFinite(Date.parse(value));
}
function isOptionalTimestamp(value) {
    return value === undefined || isTimestamp(value);
}
function isNonEmptyString(value) {
    return typeof value === "string" && value.length > 0;
}
function isObservation(value) {
    if (!isRecord(value))
        return false;
    try {
        validateClaimObservation(value);
        return true;
    }
    catch {
        return false;
    }
}
function isCandidate(value) {
    return (isRecord(value) &&
        value.version === 1 &&
        isNonEmptyString(value.id) &&
        isNonEmptyString(value.identity_key) &&
        isObservation(value.observation) &&
        typeof value.status === "string" &&
        CANDIDATE_STATUSES.has(value.status) &&
        isTimestamp(value.created_at) &&
        isTimestamp(value.updated_at) &&
        isOptionalTimestamp(value.resolved_at) &&
        (value.resolution_note === undefined || typeof value.resolution_note === "string"));
}
export function isClaimCandidateStore(value) {
    if (!isRecord(value) || !isRecord(value.stats))
        return false;
    return (value.version === 1 &&
        value.schema_version === 1 &&
        typeof value.project_root === "string" &&
        isTimestamp(value.created_at) &&
        isTimestamp(value.last_updated) &&
        Array.isArray(value.candidates) &&
        value.candidates.every(isCandidate) &&
        Number.isInteger(value.stats.total_candidates) &&
        Number.isInteger(value.stats.pending_count) &&
        Number.isInteger(value.stats.approved_count) &&
        Number.isInteger(value.stats.rejected_count) &&
        typeof value.size_bytes === "number" &&
        Number.isFinite(value.size_bytes));
}
export function createEmptyClaimCandidateStore(projectRoot) {
    const now = new Date().toISOString();
    return {
        version: 1,
        schema_version: 1,
        project_root: projectRoot,
        created_at: now,
        last_updated: now,
        candidates: [],
        stats: {
            total_candidates: 0,
            pending_count: 0,
            approved_count: 0,
            rejected_count: 0,
        },
        size_bytes: 0,
    };
}
export function loadClaimCandidateStore(storePath, recoverCorrupt = false) {
    const parsed = readJsonFile(storePath, false);
    if (parsed === null) {
        if (recoverCorrupt) {
            try {
                backupCorruptFile(storePath);
            }
            catch { }
        }
        return null;
    }
    if (!isClaimCandidateStore(parsed)) {
        if (recoverCorrupt) {
            try {
                backupCorruptFile(storePath);
            }
            catch { }
        }
        return null;
    }
    return parsed;
}
export function refreshClaimCandidateStoreStats(store) {
    store.stats = {
        total_candidates: store.candidates.length,
        pending_count: store.candidates.filter((candidate) => candidate.status === "pending").length,
        approved_count: store.candidates.filter((candidate) => candidate.status === "approved").length,
        rejected_count: store.candidates.filter((candidate) => candidate.status === "rejected").length,
    };
}
export function saveClaimCandidateStore(storePath, store) {
    refreshClaimCandidateStoreStats(store);
    store.last_updated = new Date().toISOString();
    store.size_bytes = Buffer.byteLength(JSON.stringify(store), "utf-8");
    writeJsonAtomic(storePath, store);
}
export function createClaimCandidate(observation, identityKey, now = new Date().toISOString()) {
    validateClaimObservation(observation);
    return {
        id: `can-${crypto.randomUUID()}`,
        version: 1,
        identity_key: identityKey,
        observation: structuredClone(observation),
        status: "pending",
        created_at: now,
        updated_at: now,
    };
}
//# sourceMappingURL=claim-candidate-store.js.map