#pragma once
#include <cassert>
#include "ControllerButtonToActionMap.h"
#include "KeyboardTranslationHelpers.h"

namespace sds
{
	/**
	 * \brief	A logical representation of a mapping's exclusivity group activation status.
	 */
	struct GroupActivationInfo final
	{
		// Exclusivity grouping value, mirroring the mapping value used.
		keyboardtypes::GrpVal_t GroupingValue{};
		// A value of 0 indicates no mapping is activated.
		std::size_t ActivatedMappingHash{};
		// Necessary to prevent switching between down/up repeatedly.
		keyboardtypes::SmallVector_t<std::size_t> OvertakenHashes;
	};

	struct FilteredPair
	{
		std::optional<TranslationResult> Original;
		std::optional<TranslationResult> Overtaking;
	};

	[[nodiscard]]
	constexpr
	auto GetActivatedGroupingInfo(const std::span<GroupActivationInfo> groupRange, const std::integral auto groupValue, const std::integral auto hashToMatch)
	{
		using std::ranges::find_if, std::ranges::cend;
		const auto findResult = find_if(groupRange, [groupValue, hashToMatch](const GroupActivationInfo& e)
			{
				return e.ActivatedMappingHash == hashToMatch && e.GroupingValue == groupValue;
			});
		return std::make_pair(findResult != cend(groupRange), findResult);
	}

	[[nodiscard]]
	constexpr
	auto GetGroupInfoForUnsetHashcode(const GroupActivationInfo& existingGroup) -> GroupActivationInfo
	{
		return GroupActivationInfo{ existingGroup.GroupingValue, {}, {} };
	}

	[[nodiscard]]
	constexpr
	auto GetGroupInfoForNewSetHashcode(const GroupActivationInfo& existingGroup, const std::size_t newlyActivatedHash)
	{
		return GroupActivationInfo{ existingGroup.GroupingValue, newlyActivatedHash, {} };
	}


	/**
	 * \brief	May be used to internally filter the poller's translations in order to apply the overtaking behavior.
	 */
	class KeyboardOvertakingFilter final
	{
		// Constructed from the mapping list, pairs with ex. group data.
		keyboardtypes::SmallVector_t<GroupActivationInfo> m_exclusivityGroupInfo;
		// span to mappings
		std::span<CBActionMap> m_mappings;
		KeyboardSettingsPack m_settings;
	public:
		auto GetUpdatedState(const keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>& stateUpdate) -> keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>
		{

			// TODO parse all of the down key VKs based on overtaking behavior.
			return {};
		}

		void SetMappingRange(std::span<CBActionMap> mappingsList)
		{
			m_mappings = mappingsList;
			m_exclusivityGroupInfo = {};
			// Build the buffer of ex. group information.
			for (const auto& elem : mappingsList)
			{
				if (elem.ExclusivityGrouping)
				{
					m_exclusivityGroupInfo.emplace_back(
						GroupActivationInfo
						{
							.GroupingValue = *elem.ExclusivityGrouping,
							.ActivatedMappingHash = 0,
							.OvertakenHashes = {}
						});
				}
			}
		}

		auto FilterDownTranslation(TranslationResult&& translation) -> FilteredPair
		{
			using std::ranges::find;
			using std::ranges::find_if;
			using std::ranges::cend;

			if (!translation.ExclusivityGrouping)
			{
				return { .Original = std::move(translation), .Overtaking = {} };
			}

			const auto groupIndex = GetGroupingInfoIndex(*translation.ExclusivityGrouping);
			assert(groupIndex.has_value());
			const auto foundResult = find(m_exclusivityGroupInfo[*groupIndex].OvertakenHashes, translation.MappingHash);
			const bool isTranslationAlreadyOvertaken = foundResult != cend(m_exclusivityGroupInfo[*groupIndex].OvertakenHashes);
			const bool isTranslationAlreadyActivated = m_exclusivityGroupInfo[*groupIndex].ActivatedMappingHash == translation.MappingHash;
			const bool isNoMappingHashSet = !m_exclusivityGroupInfo[*groupIndex].ActivatedMappingHash;
			const auto currentMapping = find_if(m_mappings, [&](const auto& elem)
				{
					const auto elemHash = hash_value(elem);
					return translation.MappingHash == elemHash;
				});
			assert(currentMapping != cend(m_mappings));

			FilteredPair filteredPair;
			// Handle new down
			if (isNoMappingHashSet)
			{
				UpdateGroupInfoForNewDown(translation, *groupIndex);
				filteredPair.Original = std::move(translation);
			}
			// Handle overtaking down translation, not one of the already overtaken.
			else if (!isTranslationAlreadyActivated && !isTranslationAlreadyOvertaken)
			{
				const auto overtakenMappingIndex = GetMappingIndexByHash(m_exclusivityGroupInfo[*groupIndex].ActivatedMappingHash);
				UpdateGroupInfoForOvertakingDown(translation, *groupIndex);
				filteredPair.Overtaking = GetKeyUpTranslationResult(m_mappings[overtakenMappingIndex]);
				filteredPair.Original = std::move(translation);
			}
			return filteredPair;
		}

		auto FilterUpTranslation(const TranslationResult& translation) -> FilteredPair
		{
			using std::ranges::find,
				std::ranges::end,
				std::ranges::find_if,
				std::ranges::cend;

			if (!translation.ExclusivityGrouping)
				return { .Original = translation };

			const auto groupIndex = GetGroupingInfoIndex(*translation.ExclusivityGrouping);
			assert(groupIndex.has_value());
			auto& currentGroupInfo = m_exclusivityGroupInfo[*groupIndex];

			const auto foundResult = find(currentGroupInfo.OvertakenHashes, translation.MappingHash);
			const bool isTranslationAlreadyOvertaken = foundResult != cend(currentGroupInfo.OvertakenHashes);
			const bool isTranslationAlreadyActivated = currentGroupInfo.ActivatedMappingHash == translation.MappingHash;
			const bool hasWaitingOvertaken = !currentGroupInfo.OvertakenHashes.empty();

			FilteredPair filteredUp{ .Original = translation };
			if (isTranslationAlreadyActivated)
			{
				// Remove from activated slot, replace with next in line
				currentGroupInfo.ActivatedMappingHash = {};
				if (hasWaitingOvertaken)
				{
					currentGroupInfo.ActivatedMappingHash = *currentGroupInfo.OvertakenHashes.cbegin();
					currentGroupInfo.OvertakenHashes.erase(currentGroupInfo.OvertakenHashes.cbegin());
					// Prepare key-down for it.
					const auto newDownMappingIndex = GetMappingIndexByHash(currentGroupInfo.ActivatedMappingHash);
					filteredUp.Overtaking = GetInitialKeyDownTranslationResult(m_mappings[newDownMappingIndex]);
				}
			}
			else if (isTranslationAlreadyOvertaken)
			{
				// Does not require sending a key-up for these.
				currentGroupInfo.OvertakenHashes.erase(foundResult);
			}

			return filteredUp;
		}
	private:
		[[nodiscard]]
		constexpr
		auto GetMappingIndexByHash(const std::size_t hash) const -> std::size_t
		{
			for (std::size_t i{}; i < m_mappings.size(); ++i)
			{
				if (hash_value(m_mappings[i]) == hash)
					return i;
			}
			assert(false);
			return {};
		}

		[[nodiscard]]
		auto GetGroupingInfoIndex(const std::integral auto exclusivityGroupValue) -> std::optional<std::size_t>
		{
			for (std::size_t i{}; i < m_exclusivityGroupInfo.size(); ++i)
			{
				if (m_exclusivityGroupInfo[i].GroupingValue == exclusivityGroupValue)
					return i;
			}
			return {};
		}

		// Handle not activated grouping (set hash-code as activated for the grouping)
		void UpdateGroupInfoForNewDown(const TranslationResult& translation, const std::size_t groupIndex)
		{
			m_exclusivityGroupInfo[groupIndex] = GetGroupInfoForNewSetHashcode(m_exclusivityGroupInfo[groupIndex], translation.MappingHash);
		}

		// Handle overtaking down translation
		void UpdateGroupInfoForOvertakingDown(const TranslationResult& translation, const std::size_t groupIndex)
		{
			auto& currentRange = m_exclusivityGroupInfo[groupIndex].OvertakenHashes;
			currentRange.emplace_back(0);
			std::ranges::shift_right(currentRange, 1);
			currentRange[0] = m_exclusivityGroupInfo[groupIndex].ActivatedMappingHash;
			m_exclusivityGroupInfo[groupIndex].ActivatedMappingHash = translation.MappingHash;
			m_exclusivityGroupInfo[groupIndex].GroupingValue = m_exclusivityGroupInfo[groupIndex].GroupingValue;
		}

		// Handle up translation with matching hash-code set as activated (unset the activated grouping hash-code)
		void UpdateGroupInfoForUp(const std::size_t groupIndex)
		{
			m_exclusivityGroupInfo[groupIndex] = GetGroupInfoForUnsetHashcode(m_exclusivityGroupInfo[groupIndex]);
		}

		// Handle up translation with matching hash-code set as activated--but also more than one mapping in the overtaken buffer.
		void UpdateGroupInfoForUpOfActivatedHash(const std::size_t groupIndex)
		{
			using std::ranges::begin,
				std::ranges::cbegin,
				std::ranges::end,
				std::ranges::cend,
				std::ranges::remove;

			auto& currentGroupInfo = m_exclusivityGroupInfo[groupIndex];
			GroupActivationInfo newGroup{
				.GroupingValue = currentGroupInfo.GroupingValue,
				.ActivatedMappingHash = {},
				.OvertakenHashes = {}
			};

			auto& currentOvertakenBuffer = currentGroupInfo.OvertakenHashes;
			const auto firstElement = cbegin(currentOvertakenBuffer);
			if (firstElement != cend(currentOvertakenBuffer))
			{
				newGroup.ActivatedMappingHash = *firstElement;
				currentOvertakenBuffer.erase(firstElement);
				newGroup.OvertakenHashes = currentOvertakenBuffer;
			}

			//auto& currentGroup = m_exclusivityGroupInfo[groupIndex];
			//auto tempGroupInfo = GetGroupInfoForUnsetHashcode(currentGroup);
			//const bool isOvertakenBufferEmpty = currentGroup.OvertakenHashes.empty();
			//if(!isOvertakenBufferEmpty)
			//{
			//	const auto hashedValueForFirstInLine = *currentGroup.OvertakenHashes.begin();
			//	currentGroup.OvertakenHashes.erase(currentGroup.OvertakenHashes.begin());
			//	tempGroupInfo.ActivatedMappingHash = hashedValueForFirstInLine;
			//}
			//tempGroupInfo.OvertakenHashes = m_exclusivityGroupInfo[groupIndex].OvertakenHashes;
			//m_exclusivityGroupInfo[groupIndex] = tempGroupInfo;
		}
	};

}
