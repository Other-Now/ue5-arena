# Engine concepts, and where this project touches them

These are short interview notes. Each one says what the engine does and then points to the line in this repo that depends on it.

## Frame order and tick groups
- A frame goes: input → tick groups in order (`TG_PrePhysics`, `TG_StartPhysics`, `TG_DuringPhysics`, `TG_EndPhysics`, `TG_PostPhysics`, `TG_PostUpdateWork`) → timers → net replication (`UNetDriver::TickFlush`, *after* gameplay) → render thread picks up the frame.
- Actors and components tick in `TG_PrePhysics` by default.
- `CharacterMovementComponent` also ticks in PrePhysics, which is why gameplay that reads a pawn's position sees the result of *this* frame's movement only if it ticks later.
- **Here:**
  - Nothing ticks except the AI controller, which ticks for focus rotation only.
  - Health and Weapon set `bCanEverTick = false`.
  - Bot decisions and reload use `FTimerManager`.
  - `arena.BotThinkOnTick` exists to show the cost difference.

## UObject lifetime and garbage collection
- A UObject survives only if it is reachable from the root set through **`UPROPERTY`** references (or `TStrongObjectPtr`/`AddReferencedObjects`).
- A raw `UObject*` member without `UPROPERTY` is invisible to GC. It will dangle after the next collection.
- **Here:**
  - `UArenaProjectilePool::Free` is a `UPROPERTY() TArray<TObjectPtr<>>`.
  - The input actions on `AArenaPlayerController` are `UPROPERTY`, so they stay alive across pawn respawns.
- `TWeakObjectPtr` is for references we observe but don't own. It becomes null when the object dies, and it doesn't keep the object alive.
  - **Here:** the bot's `Target`, and the respawn lambda's controller (the player can disconnect during the delay).
- `IsValid(Obj)` checks for both null and pending-kill. The pool checks it before reusing, because actors can be destroyed from outside (level streaming, `Destroy()` by other code).

## Actor and component lifecycle
- Spawn: constructor (CDO) → `PostActorCreated` → `PostInitializeComponents` → `BeginPlay`.
- Teardown: `EndPlay` → `Destroyed` → GC.
- Constructors run for the **class default object** too. Never touch the world there. That's why:
  - `ConstructorHelpers::FObjectFinder` is used only for engine meshes.
  - Delegate binding happens in `BeginPlay` (`AArenaProjectile::BeginPlay`, `UArenaHealthComponent::BeginPlay`).
- Components created at runtime (`NewObject` + `RegisterComponent`) versus default subobjects (`CreateDefaultSubobject` in the constructor, which get serialised with the class). **Here:** the weapon mesh is runtime-created after an async load, and everything else is a default subobject.
- `SetLifeSpan(0.1)` instead of `Destroy()` in `OnCharacterKilled`. Death happens inside `TakeDamage → OnTakeAnyDamage → OnDied`, a callback chain that is still using the actor.

## Replication model
- Server-authoritative. The server owns game state, and clients send *requests* (server RPCs).
- Property replication is state: last value wins, it's delta-compressed, and it's sent when changed and polled at `NetUpdateFrequency`. RPCs are events.
- `ReplicatedUsing=OnRep_X` runs on clients only. The server must call the same logic itself (`UArenaHealthComponent::HandleTakeAnyDamage` broadcasts manually for the listen host).
- Conditions: `COND_OwnerOnly` for ammo, because only the owner's HUD needs it.
- Relevancy: `NetCullDistanceSquared`, `bAlwaysRelevant`, `bOnlyRelevantToOwner`.
  - A **hidden actor with collision disabled is never relevant** (`AActor::IsNetRelevantFor`).
  - That's why pooled projectiles hide the mesh component, not the actor.
- Server RPC `WithValidation` means `_Validate` returning false **disconnects** the client. Use it for impossible input (NaN, teleporting muzzle), not for gameplay "no" (out of ammo, which is just ignored).
- Reliable vs. unreliable:
  - Reliable RPCs are retransmitted and ordered, and too many in flight overflows the reliable buffer and kicks the client.
  - Fire (<10/s) is reliable. Movement (`ServerMove`) is unreliable because a newer move supersedes a lost one.

## How CharacterMovementComponent predicts
1. The owning client simulates its move immediately, saves it (`FSavedMove_Character`) and sends it compressed (`ServerMove`: acceleration, timestamp, flags, end position).
2. The server replays the move with the same inputs. If the result is within tolerance it acks, otherwise it sends `ClientAdjustPosition`.
3. On a correction the client snaps to the server state and **replays all unacked saved moves** on top.
4. Simulated proxies (other players) only get replicated movement and are smoothed (`NetworkSmoothingMode`).

This is the same pattern as an exchange order book with optimistic local state: act locally, reconcile against the authority, replay what's in flight.

## Collision
- Object channel = what I *am*. Trace channel = what a query *asks for*. Responses are Ignore / Overlap / Block, per channel pair.
- The effective response is the *least blocking* of the two sides. Hit events need Block on both sides; overlap needs at least Overlap on both.
- **Here:** a custom `Projectile` object channel with default Block, and a profile that ignores Projectile/Camera/Visibility (see `Config/DefaultEngine.ini`).
- Sweeps (`ProjectileMovementComponent`) generate `OnComponentHit` with `QueryOnly` collision, because no physics simulation is needed.

## Asset management
- Hard references (`UPROPERTY() UStaticMesh*`) load with their owner. Soft references (`TSoftObjectPtr`) are just a path until you load them.
- `FStreamableManager::RequestAsyncLoad` loads on the async loading thread and calls back on the game thread. Keep the returned handle, or the asset can be GC'd; cancel the handle in `EndPlay`.
- The cooker follows hard references automatically. For soft-only assets you must tell it: Asset Manager `PrimaryAssetTypesToScan` with `CookRule=AlwaysCook` (`Config/DefaultGame.ini`).
