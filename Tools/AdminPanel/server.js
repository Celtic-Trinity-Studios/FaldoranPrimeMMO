/**
 * Faldoran Prime MMO — Database Admin Panel Server
 * ================================================
 * A web-based admin panel for viewing and editing game database tables.
 * 
 * Reads database credentials from the UE project's DefaultGame.ini.
 * 
 * Usage:
 *   cd Tools/AdminPanel
 *   npm install
 *   npm start
 *   -> Open http://localhost:3500 in your browser
 */

const express = require('express');
const session = require('express-session');
const { Pool } = require('pg');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = 3500;

// ---------------------------------------------------------------------------
// Parse DB config from DefaultGame.ini
// ---------------------------------------------------------------------------
function loadDbConfig() {
    const iniPath = path.resolve(__dirname, '../../Config/DefaultGame.ini');
    const raw = fs.readFileSync(iniPath, 'utf-8');
    const lines = raw.split(/\r?\n/);
    const config = {};
    let inSection = false;

    for (const line of lines) {
        const trimmed = line.trim();
        if (trimmed === '[FPM.Database]') {
            inSection = true;
            continue;
        }
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inSection = false;
            continue;
        }
        if (inSection && trimmed.includes('=')) {
            const [key, ...rest] = trimmed.split('=');
            config[key.trim()] = rest.join('=').trim();
        }
    }
    return config;
}

const dbConfig = loadDbConfig();
console.log(`[AdminPanel] Connecting to PostgreSQL at ${dbConfig.Host}:${dbConfig.Port}/${dbConfig.DatabaseName}`);

const pool = new Pool({
    host: dbConfig.Host,
    port: parseInt(dbConfig.Port) || 5432,
    database: dbConfig.DatabaseName,
    user: dbConfig.Username,
    password: dbConfig.Password,
    connectionTimeoutMillis: 5000,
});

// Test connection on startup
pool.query('SELECT NOW()')
    .then(res => console.log(`[AdminPanel] ✅ Database connected — Server time: ${res.rows[0].now}`))
    .catch(err => console.error(`[AdminPanel] ❌ Database connection failed:`, err.message));

// ---------------------------------------------------------------------------
// Middleware
// ---------------------------------------------------------------------------
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(session({
    secret: 'faldoran-admin-secret-key-change-me-in-prod',
    resave: false,
    saveUninitialized: false,
    cookie: { maxAge: 3600000 } // 1 hour
}));

// Serve static files from /public
app.use(express.static(path.join(__dirname, 'public')));

// ---------------------------------------------------------------------------
// Admin Auth — simple hardcoded admin (can be expanded later)
// ---------------------------------------------------------------------------
const ADMIN_USERNAME = 'admin';
const ADMIN_PASSWORD = 'faldoran2026'; // Change this!

function requireAuth(req, res, next) {
    if (req.session && req.session.authenticated) {
        return next();
    }
    res.status(401).json({ error: 'Not authenticated' });
}

// Login
app.post('/api/login', (req, res) => {
    const { username, password } = req.body;
    if (username === ADMIN_USERNAME && password === ADMIN_PASSWORD) {
        req.session.authenticated = true;
        req.session.username = username;
        return res.json({ success: true });
    }
    res.status(401).json({ error: 'Invalid credentials' });
});

// Logout
app.post('/api/logout', (req, res) => {
    req.session.destroy();
    res.json({ success: true });
});

// Check auth status
app.get('/api/auth-status', (req, res) => {
    res.json({ authenticated: !!(req.session && req.session.authenticated) });
});

// ---------------------------------------------------------------------------
// Database Info API
// ---------------------------------------------------------------------------
app.get('/api/db-info', requireAuth, async (req, res) => {
    try {
        const dbTime = await pool.query('SELECT NOW() as server_time');
        const tableInfo = await pool.query(`
            SELECT table_name, 
                   (SELECT count(*) FROM information_schema.columns c WHERE c.table_name = t.table_name AND c.table_schema = 'public') as column_count
            FROM information_schema.tables t
            WHERE table_schema = 'public' AND table_type = 'BASE TABLE'
            ORDER BY table_name
        `);

        // Get row counts
        const tables = [];
        for (const row of tableInfo.rows) {
            const countResult = await pool.query(`SELECT count(*) as count FROM "${row.table_name}"`);
            tables.push({
                name: row.table_name,
                columns: parseInt(row.column_count),
                rows: parseInt(countResult.rows[0].count)
            });
        }

        res.json({
            host: dbConfig.Host,
            port: dbConfig.Port,
            database: dbConfig.DatabaseName,
            serverTime: dbTime.rows[0].server_time,
            tables
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// ---------------------------------------------------------------------------
// Generic Table API
// ---------------------------------------------------------------------------

// Get all rows from a table
app.get('/api/tables/:tableName', requireAuth, async (req, res) => {
    const { tableName } = req.params;
    const allowedTables = ['accounts', 'characters', 'character_affinities'];

    if (!allowedTables.includes(tableName)) {
        return res.status(400).json({ error: 'Invalid table name' });
    }

    try {
        // Get column info
        const colInfo = await pool.query(`
            SELECT column_name, data_type, is_nullable, column_default
            FROM information_schema.columns 
            WHERE table_name = $1 AND table_schema = 'public'
            ORDER BY ordinal_position
        `, [tableName]);

        // Get rows
        const rows = await pool.query(`SELECT * FROM "${tableName}" ORDER BY 1`);

        res.json({
            table: tableName,
            columns: colInfo.rows,
            rows: rows.rows,
            totalCount: rows.rowCount
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Update a cell value
app.put('/api/tables/:tableName', requireAuth, async (req, res) => {
    const { tableName } = req.params;
    const { primaryKey, column, value } = req.body;
    const allowedTables = ['accounts', 'characters', 'character_affinities'];

    if (!allowedTables.includes(tableName)) {
        return res.status(400).json({ error: 'Invalid table name' });
    }

    // Determine primary key column(s)
    const pkColumns = getPrimaryKeyColumns(tableName);

    try {
        // Build WHERE clause from primary key
        const whereClause = pkColumns.map((col, idx) => `"${col}" = $${idx + 1}`).join(' AND ');
        const pkValues = pkColumns.map(col => primaryKey[col]);

        const query = `UPDATE "${tableName}" SET "${column}" = $${pkValues.length + 1} WHERE ${whereClause} RETURNING *`;
        const result = await pool.query(query, [...pkValues, value]);

        if (result.rowCount === 0) {
            return res.status(404).json({ error: 'Row not found' });
        }

        res.json({ success: true, row: result.rows[0] });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Delete a row
app.delete('/api/tables/:tableName', requireAuth, async (req, res) => {
    const { tableName } = req.params;
    const { primaryKey } = req.body;
    const allowedTables = ['accounts', 'characters', 'character_affinities'];

    if (!allowedTables.includes(tableName)) {
        return res.status(400).json({ error: 'Invalid table name' });
    }

    const pkColumns = getPrimaryKeyColumns(tableName);

    try {
        const whereClause = pkColumns.map((col, idx) => `"${col}" = $${idx + 1}`).join(' AND ');
        const pkValues = pkColumns.map(col => primaryKey[col]);

        const result = await pool.query(`DELETE FROM "${tableName}" WHERE ${whereClause}`, pkValues);

        if (result.rowCount === 0) {
            return res.status(404).json({ error: 'Row not found' });
        }

        res.json({ success: true, deleted: result.rowCount });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Insert a new row
app.post('/api/tables/:tableName/rows', requireAuth, async (req, res) => {
    const { tableName } = req.params;
    const { row } = req.body;
    const allowedTables = ['accounts', 'characters', 'character_affinities'];

    if (!allowedTables.includes(tableName)) {
        return res.status(400).json({ error: 'Invalid table name' });
    }

    try {
        const columns = Object.keys(row).filter(k => row[k] !== '' && row[k] !== null);
        const values = columns.map(k => row[k]);
        const placeholders = columns.map((_, i) => `$${i + 1}`);

        const query = `INSERT INTO "${tableName}" (${columns.map(c => `"${c}"`).join(', ')}) VALUES (${placeholders.join(', ')}) RETURNING *`;
        const result = await pool.query(query, values);

        res.json({ success: true, row: result.rows[0] });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// ---------------------------------------------------------------------------
// Character Detail API (joined character + affinities)
// ---------------------------------------------------------------------------

// IMPORTANT: This static route must be BEFORE the parameterized :characterId routes
// Get all characters with summary info (for the character list)
app.get('/api/characters-list', requireAuth, async (req, res) => {
    try {
        const result = await pool.query(`
            SELECT c.*, a.username as account_username,
                   (SELECT count(*) FROM character_affinities ca WHERE ca.character_id = c.character_id) as affinity_count
            FROM characters c
            JOIN accounts a ON a.account_id = c.account_id
            ORDER BY c.created_at DESC
        `);
        res.json({ characters: result.rows });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Get a single character with all affinities
app.get('/api/characters/:characterId', requireAuth, async (req, res) => {
    const { characterId } = req.params;
    try {
        const charResult = await pool.query('SELECT * FROM characters WHERE character_id = $1', [characterId]);
        if (charResult.rowCount === 0) {
            return res.status(404).json({ error: 'Character not found' });
        }

        const affResult = await pool.query(
            'SELECT * FROM character_affinities WHERE character_id = $1 ORDER BY affinity_pool, affinity_type',
            [characterId]
        );

        // Also get the account username for display
        const acctResult = await pool.query(
            'SELECT username FROM accounts WHERE account_id = $1',
            [charResult.rows[0].account_id]
        );

        res.json({
            character: charResult.rows[0],
            accountUsername: acctResult.rows.length ? acctResult.rows[0].username : 'Unknown',
            affinities: affResult.rows
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Update character fields (name, appearance, etc.)
app.put('/api/characters/:characterId', requireAuth, async (req, res) => {
    const { characterId } = req.params;
    const { fields } = req.body; // { column: value, ... }

    if (!fields || Object.keys(fields).length === 0) {
        return res.status(400).json({ error: 'No fields to update' });
    }

    // Whitelist editable columns
    const editable = ['character_name', 'body_type', 'skin_color_r', 'skin_color_g', 'skin_color_b',
        'hair_style', 'hair_color_r', 'hair_color_g', 'hair_color_b'];
    const updates = {};
    for (const [col, val] of Object.entries(fields)) {
        if (editable.includes(col)) updates[col] = val;
    }

    if (Object.keys(updates).length === 0) {
        return res.status(400).json({ error: 'No valid editable fields provided' });
    }

    try {
        const setClauses = Object.keys(updates).map((col, i) => `"${col}" = $${i + 1}`);
        const values = Object.values(updates);
        values.push(characterId);

        const query = `UPDATE characters SET ${setClauses.join(', ')} WHERE character_id = $${values.length} RETURNING *`;
        const result = await pool.query(query, values);

        if (result.rowCount === 0) {
            return res.status(404).json({ error: 'Character not found' });
        }

        res.json({ success: true, character: result.rows[0] });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Update affinities for a character (bulk update)
app.put('/api/characters/:characterId/affinities', requireAuth, async (req, res) => {
    const { characterId } = req.params;
    const { affinities } = req.body; // [{ affinity_pool, affinity_type, points }, ...]

    if (!affinities || !Array.isArray(affinities)) {
        return res.status(400).json({ error: 'Invalid affinities data' });
    }

    const client = await pool.connect();
    try {
        await client.query('BEGIN');

        for (const aff of affinities) {
            await client.query(
                `UPDATE character_affinities SET points = $1 
                 WHERE character_id = $2 AND affinity_pool = $3 AND affinity_type = $4`,
                [aff.points, characterId, aff.affinity_pool, aff.affinity_type]
            );
        }

        await client.query('COMMIT');

        // Return updated affinities
        const result = await pool.query(
            'SELECT * FROM character_affinities WHERE character_id = $1 ORDER BY affinity_pool, affinity_type',
            [characterId]
        );

        res.json({ success: true, affinities: result.rows });
    } catch (err) {
        await client.query('ROLLBACK');
        res.status(500).json({ error: err.message });
    } finally {
        client.release();
    }
});

// Execute raw SQL (admin tool — be careful!)
app.post('/api/query', requireAuth, async (req, res) => {
    const { sql } = req.body;

    if (!sql || !sql.trim()) {
        return res.status(400).json({ error: 'Empty query' });
    }

    try {
        const result = await pool.query(sql);
        res.json({
            success: true,
            command: result.command,
            rowCount: result.rowCount,
            rows: result.rows || [],
            fields: result.fields ? result.fields.map(f => ({ name: f.name, dataTypeID: f.dataTypeID })) : []
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function getPrimaryKeyColumns(tableName) {
    switch (tableName) {
        case 'accounts': return ['account_id'];
        case 'characters': return ['character_id'];
        case 'character_affinities': return ['character_id', 'affinity_pool', 'affinity_type'];
        default: return ['id'];
    }
}

// ---------------------------------------------------------------------------
// SPA fallback — serve index.html for all non-API routes
// ---------------------------------------------------------------------------
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
app.listen(PORT, () => {
    console.log(`[AdminPanel] 🚀 Faldoran Prime Admin Panel running at http://localhost:${PORT}`);
    console.log(`[AdminPanel] Login with admin / faldoran2026`);
});
