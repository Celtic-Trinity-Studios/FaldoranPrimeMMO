-- Faldoran Prime MMO — Migration 004: Equipment Table
-- Date: 2026-03-03
-- Purpose: Create the persistent equipment table for piecemeal body-slot gear.
--
-- Each row = one equipped item in a specific body slot for a character.
-- slot = SMALLINT mapping to EFPMEquipSlot enum (1..26), 0=None is never stored.
-- rarity = SMALLINT mapping to EFPMItemRarity enum (0=Common..4=Legendary).
--
-- Run in HeidiSQL / pgAdmin against the faldoran_prime database.
-- Safe to run multiple times (CREATE TABLE IF NOT EXISTS).
-- -----------------------------------------------------------------

CREATE TABLE IF NOT EXISTS equipment (
    id           SERIAL          PRIMARY KEY,
    character_id UUID            NOT NULL
                                 REFERENCES characters(character_id) ON DELETE CASCADE,
    slot         SMALLINT        NOT NULL CHECK (slot > 0),
    item_id      VARCHAR(64)     NOT NULL,
    rarity       SMALLINT        NOT NULL DEFAULT 0 CHECK (rarity >= 0 AND rarity <= 4),

    -- A character can only have one item per equipment slot.
    UNIQUE (character_id, slot)
);

-- Fast lookup by character (the primary access pattern).
CREATE INDEX IF NOT EXISTS idx_equipment_character_id
    ON equipment (character_id);

-- Optional: verify the result
-- SELECT * FROM equipment WHERE character_id = <id>;
