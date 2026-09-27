# Measurements

Every number here has an A/B switch in code, so each "before" and "after" comes from the same build.
Fill the tables in as you measure. **Leave a cell as `TBD` until you have measured it.**

Record the setup once:

| Machine | Engine | Build config | Map | Resolution |
|---|---|---|---|---|
| TBD (e.g. M2 Pro, 16 GB) | 5.5.x | Development (packaged) | Arena | TBD |

Use a packaged Development build or Standalone for frame times. PIE carries editor overhead on the
same game thread and inflates everything.

---

## 1. Projectile pooling: game-thread ms

**Switch:** `arena.UsePool 1|0`
**Load:** `arena.Stress <shots/s> 10`, typed on the server or listen-host console.

1. `open Arena?listen?Bots=0`, then `stat unit`.
2. `arena.UsePool 0`, then `arena.Stress 500 10`. Read the **Game** ms after it settles, about 3 seconds in.
3. `arena.UsePool 1`, then `arena.Stress 500 10`. Read again.
4. Repeat at 1000 and 2000 shots/s.
5. For the breakdown, run Unreal Insights with the build launched with `-trace=cpu,frame`. Compare
   the `ArenaPool_Spawn` and `ArenaPool_Launch` scopes against `SpawnActor` / `DestroyActor`, and
   note the GC spikes.

| Shots/s | Game ms, pool OFF | Game ms, pool ON | Spike (max frame) OFF / ON |
|---|---|---|---|
| 500 | TBD | TBD | TBD |
| 1000 | TBD | TBD | TBD |
| 2000 | TBD | TBD | TBD |

Frame budget is 16.6 ms at 60 fps.

## 2. Bot thinking: tick vs. timer

**Switch:** `arena.BotThinkOnTick 1|0`. It applies to bots possessed after the change, so
set it on the command line with `-ExecCmds="arena.BotThinkOnTick 1"` or restart the map.

1. `open Arena?listen?Bots=32`, then `stat unit` / Insights. Look at the `ArenaBot_Think` scope and total Game ms.

| Bots | Game ms, per-frame think | Game ms, 0.25 s staggered timer | `ArenaBot_Think` calls/frame |
|---|---|---|---|
| 32 | TBD | TBD | TBD |
| 64 | TBD | TBD | TBD |

Caveat to state if asked: in tick mode, bots also *request fire* every frame. The weapon cooldown
caps actual shots at about 6.7/s, against 4/s in timer mode. So a little of the difference is
projectile work, not think work. The `ArenaBot_Think` scope isolates the think cost itself.

## 3. Bandwidth per client: projectile replication

**Switch:** `arena.ProjectileReplicateMovement 1|0`
- `1`: stream movement at 30 Hz (the naive baseline).
- `0`: one quantised launch state per shot, and clients simulate the flight themselves.

1. Launch a listen server plus one client. In the editor use Net Mode "Play As Listen Server"
   with 2 players, or run two standalone processes and `open 127.0.0.1` on the client.
2. On the server: `arena.Stress 200 20`. On the client: `stat net`, then read **Out Rate** on the
   server / **In Rate** on the client, in bytes/s.
3. For a per-property breakdown, run Networking Insights with the server launched with
   `-NetTrace=1 -trace=net`.

| Shots/s | Client in-rate, movement streamed | Client in-rate, launch-state only | Reduction |
|---|---|---|---|
| 200 | TBD | TBD | TBD |

Also worth one row: idle bandwidth per client with 8 bots running around (character movement,
`NetUpdateFrequency` 60 vs. the default 100).

## 4. Playability under network emulation

Editor: **Editor Preferences → Level Editor → Play → Multiplayer Options → Enable Network Emulation**.
Or use console commands on the client:

```
NetEmulation.PktLag 100          ; ms one-way added latency
NetEmulation.PktLagVariance 20   ; jitter
NetEmulation.PktLoss 5           ; % loss
```

Play two minutes at each setting. Write down what breaks first, for example rubber-banding,
missed hits, or `ServerFire_Validate` kicks. Kicks would show up as a client disconnect in the log.

| Added lag (ms) | Loss (%) | Movement | Hits register | Notes |
|---|---|---|---|---|
| 0 | 0 | TBD | TBD | |
| 100 | 2 | TBD | TBD | |
| 200 | 5 | TBD | TBD | |

## 5. Tests and packaging

```
UnrealEditor-Cmd ArenaGame.uproject -ExecCmds="Automation RunTests ArenaGame; Quit" -unattended -nopause -NullRHI -log
```

| Automation tests passing | Packaged platform | Package size |
|---|---|---|
| TBD / 4 | TBD (Mac arm64) | TBD |
