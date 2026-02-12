/**
 * Faldoran Prime — Admin Panel Client
 * ====================================
 */

// Affinity name mappings from UE data contract
const PLAYSTYLE_NAMES = ['Martial', 'Ranged', 'Magic', 'Crafting', 'Social', 'Survival'];
const MAGICAL_NAMES = ['Fire', 'Water', 'Earth', 'Air', 'Light', 'Shadow', 'Nature', 'Arcane'];

const PLAYSTYLE_ICONS = ['⚔️', '🏹', '✨', '🔨', '💬', '🌿'];
const MAGICAL_ICONS = ['🔥', '💧', '🪨', '💨', '☀️', '🌑', '🍃', '🔮'];

// State
let currentView = 'overview';
let tableData = {};
let editState = null;
let deleteState = null;
let characterDetailState = null;

const primaryKeys = {
    'accounts': ['account_id'],
    'characters': ['character_id'],
    'character_affinities': ['character_id', 'affinity_pool', 'affinity_type']
};
const readonlyColumns = ['account_id', 'character_id', 'created_at'];

// ============================================================
// API
// ============================================================
async function api(method, url, body = null) {
    const opts = { method, headers: { 'Content-Type': 'application/json' } };
    if (body) opts.body = JSON.stringify(body);
    const res = await fetch(url, opts);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
    return data;
}

// ============================================================
// Toast
// ============================================================
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    const icons = { success: '✅', error: '❌', info: 'ℹ️' };
    toast.innerHTML = `<span>${icons[type] || ''}</span><span>${message}</span>`;
    container.appendChild(toast);
    setTimeout(() => { toast.classList.add('toast-out'); setTimeout(() => toast.remove(), 300); }, 3500);
}

// ============================================================
// Auth
// ============================================================
async function checkAuth() {
    try {
        const data = await api('GET', '/api/auth-status');
        if (data.authenticated) showDashboard();
    } catch (e) { /* stay on login */ }
}

async function handleLogin(e) {
    e.preventDefault();
    const btn = document.getElementById('login-btn');
    const errorEl = document.getElementById('login-error');
    const username = document.getElementById('login-username').value;
    const password = document.getElementById('login-password').value;
    btn.querySelector('.btn-text').style.display = 'none';
    btn.querySelector('.btn-loader').style.display = 'inline-block';
    errorEl.textContent = '';
    try {
        await api('POST', '/api/login', { username, password });
        showDashboard();
        showToast('Welcome back, Admin!', 'success');
    } catch (err) {
        errorEl.textContent = err.message;
    } finally {
        btn.querySelector('.btn-text').style.display = '';
        btn.querySelector('.btn-loader').style.display = 'none';
    }
}

async function handleLogout() {
    try { await api('POST', '/api/logout'); } catch (e) { }
    document.getElementById('login-screen').classList.add('active');
    document.getElementById('dashboard-screen').classList.remove('active');
    document.getElementById('login-username').value = '';
    document.getElementById('login-password').value = '';
}

function showDashboard() {
    document.getElementById('login-screen').classList.remove('active');
    document.getElementById('dashboard-screen').classList.add('active');
    loadOverview();
}

// ============================================================
// Navigation
// ============================================================
function switchView(viewName) {
    document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
    const viewEl = document.getElementById(`view-${viewName}`);
    const navEl = document.getElementById(`nav-${viewName}`);
    if (viewEl) viewEl.classList.add('active');
    if (navEl) navEl.classList.add('active');
    currentView = viewName;

    if (viewName === 'accounts') loadTable('accounts');
    if (viewName === 'characters') loadCharactersList();
}

// ============================================================
// Overview
// ============================================================
async function loadOverview() {
    try {
        const data = await api('GET', '/api/db-info');
        const totalRows = data.tables.reduce((sum, t) => sum + t.rows, 0);
        document.getElementById('stats-grid').innerHTML = `
            <div class="stat-card"><div class="stat-label">Total Tables</div><div class="stat-value">${data.tables.length}</div></div>
            <div class="stat-card"><div class="stat-label">Total Rows</div><div class="stat-value">${totalRows}</div></div>
            <div class="stat-card"><div class="stat-label">Accounts</div><div class="stat-value">${data.tables.find(t => t.name === 'accounts')?.rows || 0}</div></div>
            <div class="stat-card"><div class="stat-label">Characters</div><div class="stat-value">${data.tables.find(t => t.name === 'characters')?.rows || 0}</div></div>
        `;
        document.getElementById('connection-info').innerHTML = `
            <div class="info-row"><span class="info-label">Host</span><span class="info-value">${data.host}</span></div>
            <div class="info-row"><span class="info-label">Port</span><span class="info-value">${data.port}</span></div>
            <div class="info-row"><span class="info-label">Database</span><span class="info-value">${data.database}</span></div>
            <div class="info-row"><span class="info-label">Server Time</span><span class="info-value">${new Date(data.serverTime).toLocaleString()}</span></div>
        `;
        document.getElementById('tables-summary').innerHTML = data.tables.map(t => `
            <div class="info-row"><span class="info-label">${t.name}</span><span class="info-value">${t.rows} rows · ${t.columns} cols</span></div>
        `).join('');
    } catch (err) {
        showToast('Failed to load overview: ' + err.message, 'error');
    }
}

// ============================================================
// Accounts Table (generic table rendering)
// ============================================================
async function loadTable(tableName) {
    const containerId = `${tableName}-table-container`;
    const container = document.getElementById(containerId);
    const countEl = document.getElementById(`${tableName}-count`);
    if (container) container.innerHTML = '<div class="loading-spinner">Loading data...</div>';
    try {
        const data = await api('GET', `/api/tables/${tableName}`);
        tableData[tableName] = data;
        if (countEl) countEl.textContent = `${data.totalCount} row${data.totalCount !== 1 ? 's' : ''}`;
        if (container) container.innerHTML = renderTable(tableName, data);
    } catch (err) {
        if (container) container.innerHTML = `<div class="loading-spinner" style="color:var(--accent-red)">Error: ${err.message}</div>`;
        showToast('Failed to load table: ' + err.message, 'error');
    }
}

function renderTable(tableName, data) {
    if (!data.rows.length) return '<div class="loading-spinner">No data found — table is empty.</div>';
    const columns = data.columns.map(c => c.column_name);
    let html = '<div class="table-scroll"><table><thead><tr><th>Actions</th>';
    columns.forEach(col => { html += `<th>${formatColumnName(col)}</th>`; });
    html += '</tr></thead><tbody>';
    data.rows.forEach((row, rowIdx) => {
        html += '<tr>';
        html += `<td class="cell-actions">
            <button class="btn-icon" title="Edit" onclick="openEditModal('${tableName}', ${rowIdx})">✏️</button>
            <button class="btn-icon" title="Delete" onclick="openDeleteModal('${tableName}', ${rowIdx})">🗑️</button>
        </td>`;
        columns.forEach(col => {
            const val = row[col];
            let cls = '', content = '';
            if (val === null || val === undefined) { cls = 'cell-null'; content = 'NULL'; }
            else if (col.endsWith('_id') && typeof val === 'string' && val.length > 30) { cls = 'cell-uuid'; content = val; }
            else if (col === 'password_hash' || col === 'password_salt') { content = '••••••••'; cls = 'cell-null'; }
            else if (typeof val === 'string' && val.includes('T') && val.includes(':')) { content = new Date(val).toLocaleString(); }
            else { content = escapeHtml(String(val)); }
            html += `<td class="${cls}">${content}</td>`;
        });
        html += '</tr>';
    });
    html += '</tbody></table></div>';
    return html;
}

// ============================================================
// Characters List (Card Grid)
// ============================================================
async function loadCharactersList() {
    const grid = document.getElementById('characters-grid');
    const countEl = document.getElementById('characters-count');
    grid.innerHTML = '<div class="loading-spinner">Loading characters...</div>';
    try {
        const data = await api('GET', '/api/characters-list');
        const chars = data.characters;
        if (countEl) countEl.textContent = `${chars.length} character${chars.length !== 1 ? 's' : ''}`;
        if (!chars.length) {
            grid.innerHTML = '<div class="loading-spinner">No characters found.</div>';
            return;
        }
        grid.innerHTML = chars.map(c => {
            const skinHex = rgbFloatToHex(c.skin_color_r, c.skin_color_g, c.skin_color_b);
            const hairHex = rgbFloatToHex(c.hair_color_r, c.hair_color_g, c.hair_color_b);
            const created = new Date(c.created_at).toLocaleDateString();
            return `
            <div class="char-card glass" onclick="openCharacterDetail('${c.character_id}')">
                <div class="char-card-header">
                    <div class="char-avatar" style="background: linear-gradient(135deg, ${skinHex}, ${hairHex})">
                        ${c.character_name.charAt(0).toUpperCase()}
                    </div>
                    <div class="char-card-info">
                        <h3 class="char-card-name">${escapeHtml(c.character_name)}</h3>
                        <span class="char-card-account">Account: ${escapeHtml(c.account_username)}</span>
                    </div>
                </div>
                <div class="char-card-stats">
                    <div class="char-card-stat">
                        <span class="char-card-stat-label">Body Type</span>
                        <span class="char-card-stat-value">${c.body_type}</span>
                    </div>
                    <div class="char-card-stat">
                        <span class="char-card-stat-label">Skin</span>
                        <span class="char-card-stat-value"><span class="cell-color-swatch" style="background:${skinHex}"></span></span>
                    </div>
                    <div class="char-card-stat">
                        <span class="char-card-stat-label">Hair</span>
                        <span class="char-card-stat-value"><span class="cell-color-swatch" style="background:${hairHex}"></span></span>
                    </div>
                    <div class="char-card-stat">
                        <span class="char-card-stat-label">Affinities</span>
                        <span class="char-card-stat-value">${c.affinity_count}</span>
                    </div>
                </div>
                <div class="char-card-footer">
                    <span>Created ${created}</span>
                    <span class="char-card-click-hint">Click to edit →</span>
                </div>
            </div>`;
        }).join('');
    } catch (err) {
        grid.innerHTML = `<div class="loading-spinner" style="color:var(--accent-red)">Error: ${err.message}</div>`;
        showToast('Failed to load characters: ' + err.message, 'error');
    }
}

// ============================================================
// Character Detail Modal
// ============================================================
async function openCharacterDetail(characterId) {
    const modal = document.getElementById('character-detail-modal');
    const body = document.getElementById('char-detail-body');
    const title = document.getElementById('char-detail-title');
    modal.style.display = 'flex';
    body.innerHTML = '<div class="loading-spinner">Loading character data...</div>';

    try {
        const data = await api('GET', `/api/characters/${characterId}`);
        const c = data.character;
        const affinities = data.affinities;
        characterDetailState = { characterId, character: c, affinities };

        title.textContent = c.character_name;
        const skinHex = rgbFloatToHex(c.skin_color_r, c.skin_color_g, c.skin_color_b);
        const hairHex = rgbFloatToHex(c.hair_color_r, c.hair_color_g, c.hair_color_b);

        // Separate affinities into pools
        const playstyle = affinities.filter(a => a.affinity_pool === 0).sort((a, b) => a.affinity_type - b.affinity_type);
        const magical = affinities.filter(a => a.affinity_pool === 1).sort((a, b) => a.affinity_type - b.affinity_type);

        body.innerHTML = `
            <div class="detail-sections">
                <!-- IDENTITY SECTION -->
                <div class="detail-section">
                    <h3 class="detail-section-title">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/></svg>
                        Identity
                    </h3>
                    <div class="detail-fields">
                        <div class="detail-field">
                            <label>Character Name</label>
                            <input type="text" id="detail-character_name" value="${escapeAttr(c.character_name)}" data-original="${escapeAttr(c.character_name)}">
                        </div>
                        <div class="detail-field">
                            <label>Account</label>
                            <input type="text" value="${escapeAttr(data.accountUsername)}" disabled style="opacity:0.5">
                        </div>
                        <div class="detail-field">
                            <label>Character ID</label>
                            <input type="text" value="${c.character_id}" disabled style="opacity:0.5; font-size:0.75rem">
                        </div>
                    </div>
                </div>

                <!-- APPEARANCE SECTION -->
                <div class="detail-section">
                    <h3 class="detail-section-title">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M8 14s1.5 2 4 2 4-2 4-2"/><line x1="9" y1="9" x2="9.01" y2="9"/><line x1="15" y1="9" x2="15.01" y2="9"/></svg>
                        Appearance
                    </h3>
                    <div class="detail-fields">
                        <div class="detail-field">
                            <label>Body Type</label>
                            <input type="number" id="detail-body_type" value="${c.body_type}" min="0" max="3" data-original="${c.body_type}">
                        </div>
                        <div class="detail-field">
                            <label>Hair Style</label>
                            <input type="number" id="detail-hair_style" value="${c.hair_style}" min="0" data-original="${c.hair_style}">
                        </div>
                    </div>
                    <div class="detail-color-row">
                        <div class="detail-color-group">
                            <label>Skin Color <span class="cell-color-swatch" style="background:${skinHex}"></span></label>
                            <div class="detail-color-inputs">
                                <div class="detail-field-mini"><span>R</span><input type="number" id="detail-skin_color_r" value="${c.skin_color_r}" step="0.01" min="0" max="1" data-original="${c.skin_color_r}"></div>
                                <div class="detail-field-mini"><span>G</span><input type="number" id="detail-skin_color_g" value="${c.skin_color_g}" step="0.01" min="0" max="1" data-original="${c.skin_color_g}"></div>
                                <div class="detail-field-mini"><span>B</span><input type="number" id="detail-skin_color_b" value="${c.skin_color_b}" step="0.01" min="0" max="1" data-original="${c.skin_color_b}"></div>
                            </div>
                        </div>
                        <div class="detail-color-group">
                            <label>Hair Color <span class="cell-color-swatch" style="background:${hairHex}"></span></label>
                            <div class="detail-color-inputs">
                                <div class="detail-field-mini"><span>R</span><input type="number" id="detail-hair_color_r" value="${c.hair_color_r}" step="0.01" min="0" max="1" data-original="${c.hair_color_r}"></div>
                                <div class="detail-field-mini"><span>G</span><input type="number" id="detail-hair_color_g" value="${c.hair_color_g}" step="0.01" min="0" max="1" data-original="${c.hair_color_g}"></div>
                                <div class="detail-field-mini"><span>B</span><input type="number" id="detail-hair_color_b" value="${c.hair_color_b}" step="0.01" min="0" max="1" data-original="${c.hair_color_b}"></div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- PLAYSTYLE AFFINITIES -->
                <div class="detail-section">
                    <h3 class="detail-section-title">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/></svg>
                        Playstyle Affinities <span class="detail-pool-total" id="playstyle-total">(Total: ${playstyle.reduce((s, a) => s + a.points, 0)} / 600)</span>
                    </h3>
                    <div class="affinity-grid">
                        ${playstyle.map(a => `
                            <div class="affinity-row">
                                <span class="affinity-icon">${PLAYSTYLE_ICONS[a.affinity_type] || '?'}</span>
                                <span class="affinity-name">${PLAYSTYLE_NAMES[a.affinity_type] || 'Unknown'}</span>
                                <div class="affinity-bar-wrap">
                                    <div class="affinity-bar" style="width:${Math.round((a.points / 150) * 100)}%; background: var(--gradient-primary)"></div>
                                </div>
                                <input type="number" class="affinity-input" id="aff-0-${a.affinity_type}" value="${a.points}" min="0" max="999" data-pool="0" data-type="${a.affinity_type}" data-original="${a.points}">
                            </div>
                        `).join('')}
                    </div>
                </div>

                <!-- MAGICAL AFFINITIES -->
                <div class="detail-section">
                    <h3 class="detail-section-title">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/></svg>
                        Magical Affinities <span class="detail-pool-total" id="magical-total">(Total: ${magical.reduce((s, a) => s + a.points, 0)} / 800)</span>
                    </h3>
                    <div class="affinity-grid">
                        ${magical.map(a => `
                            <div class="affinity-row">
                                <span class="affinity-icon">${MAGICAL_ICONS[a.affinity_type] || '?'}</span>
                                <span class="affinity-name">${MAGICAL_NAMES[a.affinity_type] || 'Unknown'}</span>
                                <div class="affinity-bar-wrap">
                                    <div class="affinity-bar" style="width:${Math.round((a.points / 150) * 100)}%; background: var(--gradient-accent)"></div>
                                </div>
                                <input type="number" class="affinity-input" id="aff-1-${a.affinity_type}" value="${a.points}" min="0" max="999" data-pool="1" data-type="${a.affinity_type}" data-original="${a.points}">
                            </div>
                        `).join('')}
                    </div>
                </div>
            </div>

            <!-- SAVE / DELETE BUTTONS -->
            <div class="detail-actions">
                <button class="btn btn-danger btn-sm" onclick="deleteCharacterFromDetail()">🗑️ Delete Character</button>
                <div style="flex:1"></div>
                <button class="btn btn-ghost" onclick="closeCharacterDetail()">Cancel</button>
                <button class="btn btn-primary" onclick="saveCharacterDetail()">💾 Save All Changes</button>
            </div>
        `;

        // Wire up affinity input change handlers for live total updates
        document.querySelectorAll('.affinity-input').forEach(input => {
            input.addEventListener('input', updateAffinityTotals);
        });

    } catch (err) {
        body.innerHTML = `<div class="loading-spinner" style="color:var(--accent-red)">Error: ${err.message}</div>`;
        showToast('Failed to load character: ' + err.message, 'error');
    }
}

function updateAffinityTotals() {
    let playstyleTotal = 0, magicalTotal = 0;
    document.querySelectorAll('.affinity-input[data-pool="0"]').forEach(i => { playstyleTotal += parseInt(i.value) || 0; });
    document.querySelectorAll('.affinity-input[data-pool="1"]').forEach(i => { magicalTotal += parseInt(i.value) || 0; });
    const pEl = document.getElementById('playstyle-total');
    const mEl = document.getElementById('magical-total');
    if (pEl) pEl.textContent = `(Total: ${playstyleTotal} / 600)`;
    if (mEl) mEl.textContent = `(Total: ${magicalTotal} / 800)`;
    if (pEl) pEl.style.color = playstyleTotal === 600 ? 'var(--accent-green)' : 'var(--accent-amber)';
    if (mEl) mEl.style.color = magicalTotal === 800 ? 'var(--accent-green)' : 'var(--accent-amber)';
}

async function saveCharacterDetail() {
    if (!characterDetailState) return;
    const { characterId } = characterDetailState;

    try {
        // 1. Collect changed character fields
        const charFields = ['character_name', 'body_type', 'hair_style',
            'skin_color_r', 'skin_color_g', 'skin_color_b',
            'hair_color_r', 'hair_color_g', 'hair_color_b'];
        const changes = {};
        for (const field of charFields) {
            const el = document.getElementById(`detail-${field}`);
            if (el && el.value !== el.dataset.original) {
                changes[field] = el.value;
            }
        }

        if (Object.keys(changes).length > 0) {
            await api('PUT', `/api/characters/${characterId}`, { fields: changes });
        }

        // 2. Collect changed affinities
        const affChanges = [];
        document.querySelectorAll('.affinity-input').forEach(input => {
            if (input.value !== input.dataset.original) {
                affChanges.push({
                    affinity_pool: parseInt(input.dataset.pool),
                    affinity_type: parseInt(input.dataset.type),
                    points: parseInt(input.value)
                });
            }
        });

        if (affChanges.length > 0) {
            await api('PUT', `/api/characters/${characterId}/affinities`, { affinities: affChanges });
        }

        const totalChanges = Object.keys(changes).length + affChanges.length;
        if (totalChanges > 0) {
            showToast(`Saved ${totalChanges} change${totalChanges > 1 ? 's' : ''} successfully!`, 'success');
        } else {
            showToast('No changes to save', 'info');
        }

        closeCharacterDetail();
        loadCharactersList();
        loadOverview();
    } catch (err) {
        showToast('Save failed: ' + err.message, 'error');
    }
}

function deleteCharacterFromDetail() {
    if (!characterDetailState) return;
    const { characterId, character } = characterDetailState;
    deleteState = {
        tableName: 'characters',
        pkValues: { character_id: characterId },
        displayName: character.character_name
    };
    document.getElementById('delete-msg').innerHTML = `
        Are you sure you want to delete <strong>${escapeHtml(character.character_name)}</strong>?<br>
        <br>This will also delete all associated affinities. This cannot be undone.
    `;
    document.getElementById('delete-modal').style.display = 'flex';
}

function closeCharacterDetail() {
    document.getElementById('character-detail-modal').style.display = 'none';
    characterDetailState = null;
}

// ============================================================
// Edit Modal (for accounts)
// ============================================================
function openEditModal(tableName, rowIdx) {
    const data = tableData[tableName];
    if (!data) return;
    const row = data.rows[rowIdx];
    const pk = primaryKeys[tableName];
    const pkValues = {};
    pk.forEach(col => pkValues[col] = row[col]);
    editState = { tableName, pkValues, row };
    document.getElementById('modal-title').textContent = `Edit ${formatTableName(tableName)} Row`;
    const body = document.getElementById('modal-body');
    body.innerHTML = '';
    data.columns.forEach(colInfo => {
        const col = colInfo.column_name;
        const isReadonly = readonlyColumns.includes(col) || pk.includes(col);
        const val = row[col];
        const displayVal = (val === null || val === undefined) ? '' : val;
        const div = document.createElement('div');
        div.className = 'edit-field';
        div.innerHTML = `
            <label>${formatColumnName(col)}</label>
            <input type="text" id="edit-${col}" value="${escapeAttr(String(displayVal))}" ${isReadonly ? 'disabled style="opacity:0.5"' : ''} data-column="${col}" data-original="${escapeAttr(String(displayVal))}">
            <div class="field-type">${colInfo.data_type}${colInfo.is_nullable === 'YES' ? ' (nullable)' : ''}</div>
        `;
        body.appendChild(div);
    });
    document.getElementById('edit-modal').style.display = 'flex';
}

async function saveEdit() {
    if (!editState) return;
    const { tableName, pkValues } = editState;
    try {
        const inputs = document.querySelectorAll('#modal-body input:not([disabled])');
        let changeCount = 0;
        for (const input of inputs) {
            const col = input.dataset.column;
            const newVal = input.value;
            const original = input.dataset.original;
            if (newVal !== original) {
                await api('PUT', `/api/tables/${tableName}`, { primaryKey: pkValues, column: col, value: newVal === '' ? null : newVal });
                changeCount++;
            }
        }
        if (changeCount > 0) showToast(`Updated ${changeCount} field${changeCount > 1 ? 's' : ''}`, 'success');
        closeEditModal();
        loadTable(tableName);
    } catch (err) {
        showToast('Update failed: ' + err.message, 'error');
    }
}

function closeEditModal() {
    document.getElementById('edit-modal').style.display = 'none';
    editState = null;
}

// ============================================================
// Delete Modal
// ============================================================
function openDeleteModal(tableName, rowIdx) {
    const data = tableData[tableName];
    if (!data) return;
    const row = data.rows[rowIdx];
    const pk = primaryKeys[tableName];
    const pkValues = {};
    pk.forEach(col => pkValues[col] = row[col]);
    let identifier = row.username || row.character_name || pk.map(col => `${col}: ${row[col]}`).join(', ');
    deleteState = { tableName, pkValues };
    document.getElementById('delete-msg').innerHTML = `
        Are you sure you want to delete <strong>${escapeHtml(identifier)}</strong>?<br>
        <br>This action cannot be undone${tableName === 'accounts' ? ' and will cascade-delete all associated characters and affinities' : ''}.
    `;
    document.getElementById('delete-modal').style.display = 'flex';
}

async function confirmDelete() {
    if (!deleteState) return;
    const { tableName, pkValues } = deleteState;
    try {
        await api('DELETE', `/api/tables/${tableName}`, { primaryKey: pkValues });
        showToast('Deleted successfully', 'success');
        closeDeleteModal();
        closeCharacterDetail();
        if (tableName === 'characters') loadCharactersList();
        else loadTable(tableName);
        loadOverview();
    } catch (err) {
        showToast('Delete failed: ' + err.message, 'error');
    }
}

function closeDeleteModal() {
    document.getElementById('delete-modal').style.display = 'none';
    deleteState = null;
}

// ============================================================
// SQL Query Editor
// ============================================================
async function executeQuery() {
    const sql = document.getElementById('sql-input').value.trim();
    if (!sql) return;
    const resultsDiv = document.getElementById('query-results');
    const infoSpan = document.getElementById('query-result-info');
    const tableDiv = document.getElementById('query-results-table');
    resultsDiv.style.display = 'block';
    infoSpan.textContent = 'Executing...';
    tableDiv.innerHTML = '<div class="loading-spinner">Running query...</div>';
    try {
        const data = await api('POST', '/api/query', { sql });
        infoSpan.textContent = `${data.command || 'QUERY'} — ${data.rowCount} row${data.rowCount !== 1 ? 's' : ''} affected`;
        if (data.rows && data.rows.length > 0) {
            const columns = data.fields.map(f => f.name);
            let html = '<table><thead><tr>';
            columns.forEach(col => html += `<th>${col}</th>`);
            html += '</tr></thead><tbody>';
            data.rows.forEach(row => {
                html += '<tr>';
                columns.forEach(col => {
                    const val = row[col];
                    html += val === null ? '<td class="cell-null">NULL</td>' : `<td>${escapeHtml(String(val))}</td>`;
                });
                html += '</tr>';
            });
            html += '</tbody></table>';
            tableDiv.innerHTML = html;
        } else {
            tableDiv.innerHTML = `<div class="loading-spinner" style="color:var(--accent-green)">${data.rowCount} row${data.rowCount !== 1 ? 's' : ''} affected.</div>`;
        }
        showToast('Query executed', 'success');
        if (/insert|update|delete|alter|drop|create/i.test(sql)) loadOverview();
    } catch (err) {
        infoSpan.textContent = 'ERROR';
        tableDiv.innerHTML = `<div class="loading-spinner" style="color:var(--accent-red)">${escapeHtml(err.message)}</div>`;
        showToast('Query error: ' + err.message, 'error');
    }
}

// ============================================================
// Utilities
// ============================================================
function escapeHtml(str) { const d = document.createElement('div'); d.textContent = str; return d.innerHTML; }
function escapeAttr(str) { return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }
function formatColumnName(col) { return col.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase()); }
function formatTableName(t) { return t.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase()); }
function rgbFloatToHex(r, g, b) {
    const h = v => Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16).padStart(2, '0');
    return `#${h(r)}${h(g)}${h(b)}`;
}

// ============================================================
// Event Bindings
// ============================================================
document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('login-form').addEventListener('submit', handleLogin);
    document.querySelectorAll('.nav-item[data-view]').forEach(item => {
        item.addEventListener('click', e => { e.preventDefault(); switchView(item.dataset.view); });
    });
    document.getElementById('logout-btn').addEventListener('click', handleLogout);
    document.getElementById('refresh-overview-btn').addEventListener('click', loadOverview);
    document.getElementById('refresh-characters-btn').addEventListener('click', loadCharactersList);
    document.getElementById('modal-close').addEventListener('click', closeEditModal);
    document.getElementById('modal-cancel').addEventListener('click', closeEditModal);
    document.getElementById('modal-save').addEventListener('click', saveEdit);
    document.getElementById('char-detail-close').addEventListener('click', closeCharacterDetail);
    document.getElementById('delete-modal-close').addEventListener('click', closeDeleteModal);
    document.getElementById('delete-cancel').addEventListener('click', closeDeleteModal);
    document.getElementById('delete-confirm').addEventListener('click', confirmDelete);
    document.getElementById('run-query-btn').addEventListener('click', executeQuery);
    document.getElementById('clear-query-btn').addEventListener('click', () => {
        document.getElementById('sql-input').value = '';
        document.getElementById('query-results').style.display = 'none';
    });
    document.getElementById('sql-input').addEventListener('keydown', e => {
        if (e.key === 'Enter' && e.ctrlKey) { e.preventDefault(); executeQuery(); }
    });
    document.getElementById('edit-modal').addEventListener('click', e => { if (e.target.id === 'edit-modal') closeEditModal(); });
    document.getElementById('delete-modal').addEventListener('click', e => { if (e.target.id === 'delete-modal') closeDeleteModal(); });
    document.getElementById('character-detail-modal').addEventListener('click', e => { if (e.target.id === 'character-detail-modal') closeCharacterDetail(); });
    document.addEventListener('keydown', e => { if (e.key === 'Escape') { closeEditModal(); closeDeleteModal(); closeCharacterDetail(); } });
    checkAuth();
});
