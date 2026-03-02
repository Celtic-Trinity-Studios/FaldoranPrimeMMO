-- Faldoran Prime MMO — Migration: Add Nexus Spawn Position
-- Date: 2026-03-02
-- Purpose: Add spawn_x / spawn_y / spawn_z columns to the characters table.
--          NULL = brand new character (will be sent to the continent Nexus).
--          Non-null = returning character (server restores this position).
--
-- Run this in pgAdmin against the faldoran_prime database.
-- Safe to run multiple times (uses IF NOT EXISTS guards via DO block).
-- -----------------------------------------------------------------

DO $$
BEGIN
    -- spawn_x
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_name = 'characters' AND column_name = 'spawn_x'
    ) THEN
        ALTER TABLE characters ADD COLUMN spawn_x DOUBLE PRECISION;
        RAISE NOTICE 'Added column characters.spawn_x';
    ELSE
        RAISE NOTICE 'Column characters.spawn_x already exists, skipping.';
    END IF;

    -- spawn_y
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_name = 'characters' AND column_name = 'spawn_y'
    ) THEN
        ALTER TABLE characters ADD COLUMN spawn_y DOUBLE PRECISION;
        RAISE NOTICE 'Added column characters.spawn_y';
    ELSE
        RAISE NOTICE 'Column characters.spawn_y already exists, skipping.';
    END IF;

    -- spawn_z
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_name = 'characters' AND column_name = 'spawn_z'
    ) THEN
        ALTER TABLE characters ADD COLUMN spawn_z DOUBLE PRECISION;
        RAISE NOTICE 'Added column characters.spawn_z';
    ELSE
        RAISE NOTICE 'Column characters.spawn_z already exists, skipping.';
    END IF;
END $$;

-- All three columns default to NULL.
-- Existing characters will get NULL → they'll spawn at the Nexus on first login
-- after this migration, which is exactly the desired behavior.

-- Optional: verify the result
-- SELECT column_name, data_type, is_nullable
-- FROM information_schema.columns
-- WHERE table_name = 'characters'
-- AND column_name IN ('spawn_x', 'spawn_y', 'spawn_z');
