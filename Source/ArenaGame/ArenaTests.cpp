// Unreal Automation tests. Run headless:
//   UnrealEditor-Cmd ArenaGame.uproject -ExecCmds="Automation RunTests ArenaGame; Quit"
//     -unattended -nopause -NullRHI -log
// or in the editor: Tools > Session Frontend > Automation, filter "ArenaGame".

#include "ArenaRules.h"
#include "Engine/NetSerialization.h"
#include "Misc/AutomationTest.h"
#include "UObject/CoreNet.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// A macro rather than a typed constant: EAutomationTestFlags is a namespaced
// enum before 5.5 and an enum class from 5.5 on; this spelling works for both.
#define ArenaTestFlags (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();

	// Serialises Value exactly as the replication system would and reads it
	// back. Returns the number of bits it cost on the wire.
	template <typename TQuantized>
	int64 RoundTrip(const TQuantized& Value, TQuantized& OutValue)
	{
		bool bOk = false;
		FNetBitWriter Writer(nullptr, 1024);
		const_cast<TQuantized&>(Value).NetSerialize(Writer, nullptr, bOk);
		const int64 Bits = Writer.GetNumBits();

		FNetBitReader Reader(nullptr, Writer.GetData(), Bits);
		OutValue.NetSerialize(Reader, nullptr, bOk);
		return Bits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDamageTest, "ArenaGame.Rules.Damage", ArenaTestFlags)
bool FArenaDamageTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("normal hit"), ArenaRules::ApplyDamage(100.f, 20.f, 100.f), 80.f);
	TestEqual(TEXT("overkill clamps to zero"), ArenaRules::ApplyDamage(10.f, 50.f, 100.f), 0.f);
	TestEqual(TEXT("negative damage is not a heal"), ArenaRules::ApplyDamage(50.f, -30.f, 100.f), 50.f);
	TestEqual(TEXT("NaN damage ignored"), ArenaRules::ApplyDamage(50.f, NaN, 100.f), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaCooldownTest, "ArenaGame.Rules.Cooldown", ArenaTestFlags)
bool FArenaCooldownTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("first shot"), ArenaRules::CanFire(0.0, -1e9, 0.15f));
	TestFalse(TEXT("too soon"), ArenaRules::CanFire(10.10, 10.0, 0.15f));
	TestTrue(TEXT("exactly one interval"), ArenaRules::CanFire(10.15, 10.0, 0.15f));
	// Server tolerance: jitter-bunched request at 0.13s accepted, a 2x-rate cheat is not.
	TestTrue(TEXT("jitter accepted"), ArenaRules::CanFire(10.13, 10.0, 0.15f, 0.8f));
	TestFalse(TEXT("2x rate rejected"), ArenaRules::CanFire(10.075, 10.0, 0.15f, 0.8f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaFireValidationTest, "ArenaGame.Rules.FireValidation", ArenaTestFlags)
bool FArenaFireValidationTest::RunTest(const FString& Parameters)
{
	const FVector Pawn(100.f, 200.f, 90.f);
	const FVector Fwd(1.f, 0.f, 0.f);
	TestTrue(TEXT("muzzle near pawn"), ArenaRules::IsFireRequestPlausible(Pawn, Pawn + FVector(80.f, 0.f, 40.f), Fwd, 350.f));
	TestFalse(TEXT("origin across the map"), ArenaRules::IsFireRequestPlausible(Pawn, FVector(5000.f, 0.f, 0.f), Fwd, 350.f));
	TestFalse(TEXT("non-unit direction"), ArenaRules::IsFireRequestPlausible(Pawn, Pawn, FVector(3.f, 0.f, 0.f), 350.f));
	TestFalse(TEXT("zero direction"), ArenaRules::IsFireRequestPlausible(Pawn, Pawn, FVector::ZeroVector, 350.f));
	TestFalse(TEXT("NaN origin"), ArenaRules::IsFireRequestPlausible(Pawn, FVector(NaN, 0.f, 0.f), Fwd, 350.f));
	return true;
}

// The quantised types chosen for the fire RPC and projectile launch state:
// check the precision we give up and the bits we save, and that a quantised
// direction still passes validation (otherwise ServerFire_Validate would kick
// honest clients).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaQuantizationTest, "ArenaGame.Net.Quantization", ArenaTestFlags)
bool FArenaQuantizationTest::RunTest(const FString& Parameters)
{
	const FVector_NetQuantize Location(FVector(1234.4, -567.6, 89.49));
	FVector_NetQuantize LocationOut;
	const int64 LocationBits = RoundTrip(Location, LocationOut);
	TestTrue(TEXT("location within 0.5 cm"), FVector::Dist(Location, LocationOut) <= 0.87f); // sqrt(3)*0.5
	TestTrue(TEXT("location cheaper than 3 floats (96 bits)"), LocationBits < 96);
	AddInfo(FString::Printf(TEXT("FVector_NetQuantize: %lld bits"), LocationBits));

	const FVector Dir = FVector(0.3, -0.8, 0.2).GetSafeNormal();
	const FVector_NetQuantizeNormal Normal(Dir);
	FVector_NetQuantizeNormal NormalOut;
	const int64 NormalBits = RoundTrip(Normal, NormalOut);
	TestTrue(TEXT("direction within 1e-4"), FVector::Dist(Dir, NormalOut) < 1e-4f);
	TestEqual(TEXT("normal is 3 x 16 bits"), NormalBits, int64(48));
	TestTrue(TEXT("quantised direction still passes validation"),
		ArenaRules::IsFireRequestPlausible(FVector::ZeroVector, FVector::ZeroVector, NormalOut, 350.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
