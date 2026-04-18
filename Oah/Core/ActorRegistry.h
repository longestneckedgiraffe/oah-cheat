#pragma once

#include <cstdint>
#include <vector>

#include "../Libs/UEDump/SDK.hpp"

class ActorRegistry
{
public:
	void Refresh(bool force = false);
	void Clear();
	std::uint64_t GetRevision() const { return revision; }

	const std::vector<SDK::AActor*>& GetGuards() const { return guards; }
	const std::vector<SDK::AActor*>& GetPolice() const { return police; }
	const std::vector<SDK::AActor*>& GetPlayers() const { return players; }
	const std::vector<SDK::AActor*>& GetCameras() const { return cameras; }
	const std::vector<SDK::AActor*>& GetRats() const { return rats; }
	const std::vector<SDK::AActor*>& GetDoors() const { return doors; }
	const std::vector<SDK::AActor*>& GetAlarms() const { return alarms; }
	const std::vector<SDK::AActor*>& GetLocks() const { return locks; }
	const std::vector<SDK::AActor*>& GetMoney() const { return money; }
	const std::vector<SDK::AActor*>& GetDuffelbags() const { return duffelbags; }
	const std::vector<SDK::AActor*>& GetRobberTrucks() const { return robberTrucks; }
	const std::vector<SDK::AActor*>& GetCivilians() const { return civilians; }

private:
	void Rebuild(const std::vector<SDK::ULevel*>& levels);

	std::uint32_t refreshTicks{ 0 };
	std::uint64_t revision{ 0 };

	std::vector<SDK::AActor*> guards{};
	std::vector<SDK::AActor*> police{};
	std::vector<SDK::AActor*> players{};
	std::vector<SDK::AActor*> cameras{};
	std::vector<SDK::AActor*> rats{};
	std::vector<SDK::AActor*> doors{};
	std::vector<SDK::AActor*> alarms{};
	std::vector<SDK::AActor*> locks{};
	std::vector<SDK::AActor*> money{};
	std::vector<SDK::AActor*> duffelbags{};
	std::vector<SDK::AActor*> robberTrucks{};
	std::vector<SDK::AActor*> civilians{};
};
