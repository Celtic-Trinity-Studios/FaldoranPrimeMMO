-- Faldoran Prime MMO — Initial Database Schema
-- Phase 3B: Accounts and Characters tables
-- Run this in pgAdmin Query Tool against the faldoran_prime database.
-- 
-- Prerequisites: Database "faldoran_prime" and user "fpm_server" must exist.
-- Created: 2026-02-07

-- Enable UUID generation
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- -------------------------------------------------------------------
-- Accounts Table
-- -------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS accounts (
    account_id      UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username        VARCHAR(20) NOT NULL UNIQUE,
    password_hash   TEXT NOT NULL,
    password_salt   TEXT NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login      TIMESTAMPTZ
);

-- Index for fast username lookups during login
CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts (username);

-- -------------------------------------------------------------------
-- Characters Table
-- -------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS characters (
    character_id    UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    account_id      UUID NOT NULL REFERENCES accounts(account_id) ON DELETE CASCADE,
    character_name  VARCHAR(20) NOT NULL UNIQUE,
    body_type       SMALLINT NOT NULL DEFAULT 0,
    skin_color_r    REAL NOT NULL DEFAULT 0.8,
    skin_color_g    REAL NOT NULL DEFAULT 0.6,
    skin_color_b    REAL NOT NULL DEFAULT 0.5,
    hair_style      SMALLINT NOT NULL DEFAULT 0,
    hair_color_r    REAL NOT NULL DEFAULT 0.3,
    hair_color_g    REAL NOT NULL DEFAULT 0.2,
    hair_color_b    REAL NOT NULL DEFAULT 0.1,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_played     TIMESTAMPTZ
);

-- Index for looking up all characters belonging to an account
CREATE INDEX IF NOT EXISTS idx_characters_account_id ON characters (account_id);

-- Index for checking character name uniqueness quickly
CREATE INDEX IF NOT EXISTS idx_characters_name ON characters (character_name);

-- -------------------------------------------------------------------
-- Character Affinities Table
-- -------------------------------------------------------------------
-- Stores both playstyle and magical affinity point distributions.
-- affinity_type = 'playstyle' or 'magical'
-- affinity_name = enum name (e.g. 'Martial', 'Fire', etc.)
-- points = allocated point value
CREATE TABLE IF NOT EXISTS character_affinities (
    character_id    UUID NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    affinity_type   VARCHAR(20) NOT NULL,    -- 'playstyle' or 'magical'
    affinity_name   VARCHAR(20) NOT NULL,    -- e.g. 'Martial', 'Fire'
    points          INTEGER NOT NULL DEFAULT 100,
    PRIMARY KEY (character_id, affinity_type, affinity_name)
);

-- Index for retrieving all affinities for a character
CREATE INDEX IF NOT EXISTS idx_affinities_character_id ON character_affinities (character_id);

