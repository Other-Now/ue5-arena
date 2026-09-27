# ue5-arena

A small **server-authoritative multiplayer arena shooter in Unreal Engine 5 C++**.

- Everything is in C++, with no Blueprints and no binary assets in the repo. It uses only engine basic shapes, plus one map you create in two minutes.
- The point of it is the networking and the frame budget, not the art.

> **Status:** the source is complete but has **not yet been compiled or run**. It was written on a
> machine without Unreal. Step one is building it in UE 5.5 and fixing whatever the compiler finds.
> Every performance and bandwidth figure lives in [docs/MEASURE.md](docs/MEASURE.md) and is `TBD`
> until measured. None are claimed here.

## What's in it

| System | Class | Engine features |
|---|---|---|
| Player / bot pawn | `AArenaCharacter` | `ACharacter`, CharacterMovement prediction, spring-arm camera, Enhanced Input |
| Input | `AArenaPlayerController` | Input actions and mapping context **built in code** (swizzle/negate modifiers) |
| Health | `UArenaHealthComponent` | Replicated property + `RepNotify`, `OnTakeAnyDamage`, no tick |
| Weapon | `UArenaWeaponComponent` | Server RPC `WithValidation`, server-side cooldown/ammo, owner-only replication, async-loaded soft references |
| Projectile | `AArenaProjectile` | `ProjectileMovementComponent`, custom collision channel, hit events, quantised launch-state replication |
| Pool | `UArenaProjectilePool` | `UWorldSubsystem` free list, stress command, Insights trace scopes |
| Bot | `AArenaAIController` | `AAIController`, nav-mesh `MoveToActor`, focus aiming, timer-driven 3-state machine |
| Rules | `AArenaGameMode` / `GameState` / `PlayerState` | Spawn/respawn through `RestartPlayer` (bots use the same path), kills, round clock |
| HUD | `AArenaHUD` | Canvas HUD: HP, ammo, K/D, clock, pool counters |
| Data | `UArenaWeaponData` | `UPrimaryDataAsset`, Asset Manager scan + `AlwaysCook` rule |
| Tests | `ArenaTests.cpp` | 4 Unreal Automation tests: damage, cooldown, fire validation, wire quantisation |

## Networking design

**Server authority.** Clients never apply damage, spend ammo or spawn projectiles. A client sends:

```
ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)   // reliable, WithValidation
```

The server then does two things:
- It **validates** the request: no NaN, a unit direction, and a muzzle within 350 uu of where the server thinks the pawn is. That limit is walk speed × a generous RTT. A request that fails is impossible input, and the client is disconnected.
- It **re-checks** cooldown and ammo on its own clock. A request that fails here is just ignored. The cooldown tolerance is 0.8 × interval, so shots bunched together by jitter are accepted, but a modified client can't fire more than 25% faster than intended.

**Projectiles replicate their launch, not their flight.** Each projectile carries one replicated struct `{Origin, Direction, Speed, Sequence, bActive}`:
- Origin is quantised to 1 cm and direction to 16 bits per axis.
- Clients run the same `ProjectileMovementComponent` from it locally.
- The server alone decides hits.
- `Sequence` makes a reused actor's relaunch a *changed* value, even with identical parameters, so `OnRep` fires.
- `arena.ProjectileReplicateMovement 1` switches back to streaming movement at 30 Hz, so the saving can be measured in the same build.

**Pooling.** Spawning a replicated actor per shot costs component registration, an actor-channel open/close on every client, and GC churn later. The pool keeps inactive projectiles as **invisible, non-colliding, but not hidden** actors. `AActor::IsNetRelevantFor` treats a hidden actor with collision off as irrelevant, which would stop the `bActive=false` update from reaching clients and close the channel on every reuse. `arena.UsePool 0` gives the spawn/destroy baseline.

**Bandwidth knobs.**
- Characters use `NetUpdateFrequency` 60 (default 100) with a minimum of 15.
- Projectiles use 10, with `ForceNetUpdate()` on launch and deactivate, and a 80 m cull distance.
- Ammo is `COND_OwnerOnly`.

## Trade-offs I'd defend (and what I'd change next)

- **Client-simulated projectiles vs. streamed movement.** This saves nearly all per-flight bandwidth. The cost is a client that joins mid-flight, or a projectile that becomes relevant late: the client simulates it from its origin, so it appears slightly behind where it should be. The next step is to replicate the server launch time and fast-forward by the elapsed time.
- **No lag compensation.** The server tests hits against *its* current pawn positions, so at high latency the shooter has to lead the target more. Rewinding hitboxes by the shooter's RTT fixes this and is the next feature. It is also where cheating risk goes up.
- **Reliable fire RPC.** At under 10 shots per second, reliable is cheaper than the "my shot vanished" bug under packet loss. For a 20 Hz+ weapon I'd switch to unreliable, with a sequence number and server-side reconciliation.
- **Input in code, not assets.** The bindings can be reviewed in a diff and there are no binary files. The cost is that designers can't rebind without C++. A real team would use assets.
- **Timer AI rather than a behaviour tree.** A three-state bot doesn't justify BT and Blackboard assets. Staggered 0.25 s timers spread N bots across frames. Past a handful of states I'd move to a behaviour tree or StateTree.
- **Dormancy not used yet.** Inactive pooled projectiles are still considered for replication every net tick. They send no bytes because nothing changes, but it still costs server CPU. `DORM_DormantAll` on release is the next step.

## Build and run (UE 5.5, Mac or Windows)

1. Clone, then right-click `ArenaGame.uproject` → *Generate Xcode/Visual Studio project files*, or just double-click it and let the editor build the module.
2. **Create the map once:**
   - *File → New Level → Basic*.
   - Add a few cubes as cover, one or more **Player Start** actors, and a **Nav Mesh Bounds Volume** covering the floor. Press `P` to check the green nav area.
   - Save as `/Game/Maps/Arena`.
   - Commit it. `.umap`/`.uasset` go through Git LFS: run `git lfs install` first.
3. *Optional:* create `/Game/Weapons/DA_Rifle` (Data Asset → `ArenaWeaponData`) to tune the weapon. Without it, built-in defaults are used.
4. Play:
   - **Editor:** Net Mode *Play As Listen Server*, 2 players.
   - **Console:** `open Arena?listen?Bots=8`.
   - Controls: WASD, mouse, Space, LMB.
5. Tests: `UnrealEditor-Cmd ArenaGame.uproject -ExecCmds="Automation RunTests ArenaGame; Quit" -unattended -nopause -NullRHI -log`
6. Package: *Platforms → Mac (or Windows) → Package Project*.

## Console

| Command | Effect |
|---|---|
| `arena.Stress [shots/s] [seconds]` | Server-side projectile load for profiling |
| `arena.UsePool 0/1` | Spawn/destroy per shot vs. pooled |
| `arena.ProjectileReplicateMovement 0/1` | Launch-state replication vs. streamed movement |
| `arena.BotThinkOnTick 0/1` | Staggered timer vs. per-frame bot decisions |
| `arena.PoolPrewarm N` | Projectiles pre-spawned at begin play |
| `stat unit`, `stat net` | Frame and network counters |
| `NetEmulation.PktLag 100`, `NetEmulation.PktLoss 5` | Latency and loss emulation |

## Docs

- [docs/MEASURE.md](docs/MEASURE.md): measurement procedure and result tables.
- [docs/ENGINE_NOTES.md](docs/ENGINE_NOTES.md): engine concepts (frame order, GC, lifecycle, replication, CMC prediction, collision, asset loading) mapped to this code.

## Layout

```
ArenaGame.uproject
Config/            DefaultEngine.ini (collision channel), DefaultGame.ini (asset manager), DefaultInput.ini
Source/ArenaGame/  all gameplay code + ArenaTests.cpp
docs/
```
