# WorldScribe — Price Analysis

> AI cost breakdown, competitor research, subscription tiers, and sustainability strategy.

---

## 1. AI Cost Structure

WorldScribe uses **Gemini 2.5 Flash** via **OpenRouter** for all AI simulation calls.

### OpenRouter Pricing (March 2026)

| Metric | Cost |
|---|---|
| Input tokens | **$0.30 / 1M tokens** |
| Output tokens | **$2.50 / 1M tokens** |

### API Calls Per Simulation

| Operation | Model Tier | max_tokens | When Triggered |
|---|---|---|---|
| Story Events | Smart (Gemini 2.5 Flash) | 2,048 | Per town, per month |
| Population Changes | Cheap | 2,048 | Per town, per month |
| Character Build | Cheap | 2,048 | Per new character |
| Social/Relationships | Cheap | 2,048 | Per town, per month |
| Stat/XP Updates | Cheap | 2,048 | Per town, per month |
| Full Simulation (legacy) | Smart | 65,536 | Single-town sim |
| Single Town Sim | Smart | 8,192 | Multi-town path |

---

## 2. Cost Per Simulation

Each simulation sends the full character roster + SRD reference data + rules in the prompt.  
Bigger towns = more input tokens = slightly higher cost.

### Chunked Path (4 API calls per town per simulated month)

| Town Population | Input Cost | Output Cost | Char Builds (~2 new) | **Total / Sim** |
|---|---|---|---|---|
| 10 characters | $0.0014 | $0.005 | $0.003 | **$0.009** |
| 30 characters | $0.003 | $0.005 | $0.003 | **$0.011** |
| 50 characters | $0.005 | $0.005 | $0.003 | **$0.013** |
| 100 characters | $0.008 | $0.006 | $0.003 | **$0.017** |
| 200 characters | $0.016 | $0.007 | $0.003 | **$0.026** |
| 500 characters | $0.036 | $0.010 | $0.003 | **$0.049** |
| 1,000 characters | $0.072 | $0.013 | $0.003 | **$0.088** |

### Character Intake (0-month mode)

| Characters Generated | Cost |
|---|---|
| 5 | ~$0.015 |
| 10 | ~$0.025 |
| 25 | ~$0.050 |
| 50 | ~$0.090 |

---

## 3. Competitor Pricing

| Product | Free Tier | Entry Price | Mid Tier | Top Tier | AI? |
|---|---|---|---|---|---|
| **WorldAnvil** | ✅ 2 worlds, 42 articles | $5/mo | $12/mo | $25/mo | ❌ No |
| **Worldsmith** | ❌ (7-day trial) | $5/mo | $15/mo | — | ✅ Yes (Mana credits) |
| **LitRPG Adventures** | ❌ | $5/mo (700 credits) | $50/yr | $120 lifetime | ✅ Yes (GPT) |
| **AI Game Master** | ✅ (5 tokens/4hrs) | $15/mo | $25/mo | $25/mo unlimited | ✅ Yes (GPT) |
| **Kassoon** | ✅ Basic tools | $1/mo (Patreon) | — | — | ❌ No |
| **Foundry VTT** | ❌ | $50 one-time | — | — | ❌ (modules) |

**Key takeaways:**
- $5/mo is the universal entry point for TTRPG tools
- $12–15/mo is the "serious hobbyist" sweet spot
- $25/mo is the ceiling for individual users
- NO competitor offers continuous AI town simulation — WorldScribe is unique

---

## 4. Finalized Subscription Tiers

| Tier | Price | Towns | Sims/Month | Max Chars/Town |
|---|---|---|---|---|
| **Free** | $0 | 3 | 4 (1/week) | 100 |
| **Adventurer** | $5/mo ($50/yr) | 5 | 12 (3/week) | 200 |
| **Dungeon Master** | $12/mo ($120/yr) | 15 | 40 (~daily) | 500 |
| **World Builder** | $20/mo ($200/yr) | Unlimited | 120 (4/day) | 1,000 |

**All tiers include:** SRD browser, simulation settings, character management, calendar, town history, trade routes.

### Why This Structure Works

- **Free tier is generous** — 3 towns lets users experience multi-town features (trade routes, inter-city dynamics) before paying. This is a better hook than most competitors.
- **Sim caps are the safety valve** — not character counts. A user can grow their town organically; they just can't run the simulation an unlimited number of times.
- **Natural upgrade moments:**
  - Free → Adventurer: "I want to sim more than once a week"
  - Adventurer → DM: "I have more than 5 towns in my world"
  - DM → World Builder: "I want to sim daily and build massive cities"

---

## 5. Margin Analysis

### Per-Tier Costs

| Tier | Revenue | Worst-Case AI Cost | Realistic AI Cost | Gross Margin |
|---|---|---|---|---|
| Free | $0 | $0.18/mo | $0.03/mo | Loss leader |
| Adventurer | $5/mo | $0.36/mo | $0.12/mo | **97.6%** |
| Dungeon Master | $12/mo | $2.40/mo | $0.50/mo | **95.8%** |
| World Builder | $20/mo | $10.20/mo | $1.20/mo | **94.0%** |

**Worst-case** = every town at max characters, every sim slot used.  
**Realistic** = typical usage patterns (not all towns maxed, not all sims used).

### Cost Per 1,000 Free Users

| Scenario | Monthly AI Cost | Annual Cost |
|---|---|---|
| Realistic (~20% active, light usage) | **$6** | $72 |
| All active, light usage | **$30** | $360 |
| All active, ALL maxing out | **$180** | $2,160 |

---

## 6. Sustainability Strategy — The AI Endowment Fund

AI costs are so low that **investment interest can pay for free users permanently**.

### How It Works

Put crowdfund money into a high-yield savings account (HYSA, ~4.5% APY).  
The interest pays for AI tokens. The principal is never touched.

| Principal | Annual Interest | Monthly Interest | Free Users Covered (realistic) | Free Users Covered (worst-case) |
|---|---|---|---|---|
| $1,000 | $45 | $3.75 | 125 | ~20 |
| $2,000 | $90 | $7.50 | 250 | ~42 |
| $5,000 | $225 | $18.75 | 625 | ~104 |
| **$8,000** | **$360** | **$30** | **1,000** ✅ | ~167 |
| $10,000 | $450 | $37.50 | 1,250 | ~208 |
| $48,000 | $2,160 | $180 | 6,000 | **1,000** ✅ |

**$8,000 in savings → interest covers 1,000 free users forever.**  
**$48,000 in savings → interest covers 1,000 worst-case free users forever.**

This is a powerful Kickstarter message:  
> *"Your pledge enters a sustainability fund. WorldScribe runs on interest — your world lives forever."*

---

## 7. Kickstarter Backer Tiers

| Tier | Pledge | Reward |
|---|---|---|
| **Supporter** | $5 | Name in credits + beta access |
| **Early Adventurer** | $25 | 6 months Adventurer (saves $5) |
| **Early DM** | $60 | 6 months Dungeon Master (saves $12) |
| **Founding World Builder** | $99 | 12 months World Builder (saves $141) |
| **Lifetime Adventurer** | $150 | Lifetime Adventurer access |
| **Lifetime World Builder** | $300 | Lifetime World Builder + name a town |

> ⚠️ Lifetime tiers: limit quantities (e.g. 50–100 slots). At these AI costs a lifetime user is sustainable, but hosting costs accumulate over years.

---

## 8. Summary

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  AI cost per user:    $0.03 – $1.20/month (realistic)        │
│  Subscription price:  $0 / $5 / $12 / $20 per month         │
│  Gross margins:       94 – 98%                               │
│                                                              │
│  1,000 free users cost ~$30/month in AI.                     │
│  $8K in savings covers that with interest alone.             │
│                                                              │
│  Free tier: 3 towns, 100 chars, 4 sims/mo — costs pennies.  │
│  Sim caps are the safety valve, not character limits.         │
│  AI prices only go down — margins improve over time.         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Action Items

1. Implement tier enforcement in the DB (sim counter, town/char limits)
2. Add per-user usage tracking (tokens consumed per month)
3. Set up OpenRouter billing alerts
4. Structure Kickstarter tiers based on table above
5. Earmark portion of crowdfund as AI endowment fund
6. Offer annual pricing (17% discount) to reduce churn
7. Marketing angle: *"3 towns free — build your world before you pay"*
