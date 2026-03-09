-- Faldoran Prime MMO — Migration 003: Inventory Table
-- Date: 2026-03-03
-- Purpose: Create the persistent inventory table for Tetris-style grid storage.
--
-- Each row = one item instance belonging to a character.
-- grid_x / grid_y = top-left origin on the 8×5 grid (0-based).
-- size_x / size_y = footprint (snapshotted from item definition at save time).
--
-- Run in HeidiSQL against the faldoran_prime database.
-- Safe to run multiple times (CREATE TABLE IF NOT EXISTS).
-- -----------------------------------------------------------------

CREATE TABLE IF NOT EXISTS inventory (
    id           SERIAL          PRIMARY KEY,
    character_id UUID            NOT NULL
                                 REFERENCES characters(character_id) ON DELETE CASCADE,
    item_id      VARCHAR(64)     NOT NULL,
    count        INTEGER         NOT NULL DEFAULT 1 CHECK (count > 0),
    grid_x       SMALLINT        NOT NULL DEFAULT 0 CHECK (grid_x >= 0),
    grid_y       SMALLINT        NOT NULL DEFAULT 0 CHECK (grid_y >= 0),
    size_x       SMALLINT        NOT NULL DEFAULT 1 CHECK (size_x >= 1),
    size_y       SMALLINT        NOT NULL DEFAULT 1 CHECK (size_y >= 1),

    -- A character cannot have two items with the same top-left origin.
    UNIQUE (character_id, grid_x, grid_y)
);

-- Fast lookup by character (the primary access pattern).
CREATE INDEX IF NOT EXISTS idx_inventory_character_id
    ON inventory (character_id);

-- Optional: verify the result
-- SELECT * FROM inventory WHERE character_id = <id>;
