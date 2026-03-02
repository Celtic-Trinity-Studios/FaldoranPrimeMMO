-- Faldoran Prime MMO — Schema Migration v2
-- Adds: eye color, facial morph targets, species to characters table
-- Run this in pgAdmin Query Tool against the faldoran_prime database.
--
-- Created: 2026-02-23
-- Prerequisites: Database_Schema_v1.sql already applied.

-- -------------------------------------------------------------------
-- Add eye color columns
-- -------------------------------------------------------------------
ALTER TABLE characters ADD COLUMN IF NOT EXISTS eye_color_r REAL NOT NULL DEFAULT 0.3;
ALTER TABLE characters ADD COLUMN IF NOT EXISTS eye_color_g REAL NOT NULL DEFAULT 0.5;
ALTER TABLE characters ADD COLUMN IF NOT EXISTS eye_color_b REAL NOT NULL DEFAULT 0.8;

-- -------------------------------------------------------------------
-- Add facial morph columns (0.0 - 1.0 range, default 0.5 = neutral)
-- -------------------------------------------------------------------
ALTER TABLE characters ADD COLUMN IF NOT EXISTS morph_jaw    REAL NOT NULL DEFAULT 0.5;
ALTER TABLE characters ADD COLUMN IF NOT EXISTS morph_nose   REAL NOT NULL DEFAULT 0.5;
ALTER TABLE characters ADD COLUMN IF NOT EXISTS morph_brow   REAL NOT NULL DEFAULT 0.5;
ALTER TABLE characters ADD COLUMN IF NOT EXISTS morph_lips   REAL NOT NULL DEFAULT 0.5;

-- -------------------------------------------------------------------
-- Add species column
-- Species enum values (matches EFPMSpecies in C++):
--   0 = Human (default)
--   1 = Giant
--   2 = Dwarf
--   3 = BeastKin
-- -------------------------------------------------------------------
ALTER TABLE characters ADD COLUMN IF NOT EXISTS species SMALLINT NOT NULL DEFAULT 0;
