import { backupCorruptFile, readJsonFile, writeJsonAtomic } from "./persistence.js";
const CLAIM_STATUSES = new Set(["active", "disputed", "superseded"]);
const CLAIM_RELATIONS = new Set(["confirms", "contradicts", "refines"]);
const EVIDENCE_QUALITIES = new Set([
    "automated-test",
    "reproducible-observation",
    "direct-tool-result",
    "explicit-user-correction",
    "verified-code-inspection",
    "agent-inference",
    "unverified-assumption",
]);
const PROVENANCE_SOURCES = new Set(["user", "hook", "daemon", "manual", "agent"]);
function isRecord(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}
function isString(value) {
    return typeof value === "string";
}
function isNonEmptyString(value) {
    return isString(value) && value.length > 0;
}
function isTimestamp(value) {
    return isString(value) && Number.isFinite(Date.parse(value));
}
function isStringArray(value) {
    return Array.isArray(value) && value.every(isString);
}
function isOptionalString(value) {
    return value === undefined || isString(value);
}
function isScope(value) {
    if (!isRecord(value))
        return false;
    return ((value.paths === undefined || isStringArray(value.paths)) &&
        (value.platforms === undefined || isStringArray(value.platforms)) &&
        (value.versions === undefined || isStringArray(value.versions)) &&
        (value.contexts === undefined || isStringArray(value.contexts)));
}
function isProvenance(value) {
    return (isRecord(value) &&
        typeof value.source === "string" &&
        PROVENANCE_SOURCES.has(value.source) &&
        typeof value.authority === "number" &&
        Number.isFinite(value.authority) &&
        value.authority >= 0 &&
        value.authority <= 1 &&
        isOptionalString(value.label) &&
        isNonEmptyString(value.event_id));
}
function isEvidence(value) {
    return (isRecord(value) &&
        isNonEmptyString(value.event_id) &&
        typeof value.relation === "string" &&
        CLAIM_RELATIONS.has(value.relation) &&
        typeof value.quality === "string" &&
        EVIDENCE_QUALITIES.has(value.quality) &&
        typeof value.verification_method === "string" &&
        EVIDENCE_QUALITIES.has(value.verification_method) &&
        isProvenance(value.provenance) &&
        isTimestamp(value.recorded_at) &&
        isOptionalString(value.note));
}
function isClaim(value) {
    return (isRecord(value) &&
        value.version === 1 &&
        isNonEmptyString(value.id) &&
        isNonEmptyString(value.identity_key) &&
        isNonEmptyString(value.statement) &&
        typeof value.status === "string" &&
        CLAIM_STATUSES.has(value.status) &&
        typeof value.confidence === "number" &&
        Number.isFinite(value.confidence) &&
        value.confidence >= 0 &&
        value.confidence <= 1 &&
        Array.isArray(value.evidence) &&
        value.evidence.every(isEvidence) &&
        isStringArray(value.evidence_event_ids) &&
        isStringArray(value.contradicting_event_ids) &&
        isStringArray(value.contradicts_claim_ids) &&
        isOptionalString(value.refined_from) &&
        isOptionalString(value.superseded_by) &&
        isScope(value.scope) &&
        isProvenance(value.provenance) &&
        isTimestamp(value.created_at) &&
        isTimestamp(value.updated_at));
}
export function isClaimStore(value) {
    if (!isRecord(value) || !isRecord(value.stats))
        return false;
    return (value.version === 1 &&
        value.schema_version === 1 &&
        isString(value.project_root) &&
        isTimestamp(value.created_at) &&
        isTimestamp(value.last_updated) &&
        Array.isArray(value.claims) &&
        value.claims.every(isClaim) &&
        Number.isInteger(value.stats.total_claims) &&
        Number.isInteger(value.stats.active_count) &&
        Number.isInteger(value.stats.disputed_count) &&
        Number.isInteger(value.stats.superseded_count) &&
        typeof value.size_bytes === "number" &&
        Number.isFinite(value.size_bytes));
}
export function createEmptyClaimStore(projectRoot) {
    const now = new Date().toISOString();
    return {
        version: 1,
        schema_version: 1,
        project_root: projectRoot,
        created_at: now,
        last_updated: now,
        claims: [],
        stats: {
            total_claims: 0,
            active_count: 0,
            disputed_count: 0,
            superseded_count: 0,
        },
        size_bytes: 0,
    };
}
export function loadClaimStore(storePath, recoverCorrupt = false) {
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
    if (!isClaimStore(parsed)) {
        if (recoverCorrupt)
            backupCorruptFile(storePath);
        return null;
    }
    return parsed;
}
export function refreshClaimStoreStats(store) {
    store.stats = {
        total_claims: store.claims.length,
        active_count: store.claims.filter((claim) => claim.status === "active").length,
        disputed_count: store.claims.filter((claim) => claim.status === "disputed").length,
        superseded_count: store.claims.filter((claim) => claim.status === "superseded").length,
    };
}
export function saveClaimStore(storePath, store) {
    refreshClaimStoreStats(store);
    store.last_updated = new Date().toISOString();
    store.size_bytes = Buffer.byteLength(JSON.stringify(store), "utf-8");
    writeJsonAtomic(storePath, store);
}
//# sourceMappingURL=claim-store.js.map