#pragma once
#include <stdexcept>
#include <concepts>
#include <ranges>
#include <type_traits>

#include "KeyboardCustomTypes.h"
#include "KeyboardTranslationHelpers.h"
#include "KeyboardOvertakingFilter.h"

/*
 *	Note: There are some static sized arrays used here with capacity defined in customtypes.
 */

namespace sds
{
	// A translator type, wherein you can call GetUpdatedState with a range of virtual keycode integral values,
	// and get a TranslationPack as a result.
	template<typename Poller_t>
	concept InputTranslator_c = requires(Poller_t & t)
	{
		{ t.GetUpdatedState({ 1, 2, 3 }) } -> std::convertible_to<TranslationPack>;
	};

	// Concept for range of CBActionMap type that provides random access.
	template<typename T>
	concept MappingRange_c = requires (T & t)
	{
		{ std::same_as<typename T::value_type, CBActionMap> == true };
		{ std::ranges::random_access_range<T> == true };
	};

	// Concept for a filter class, used to apply a specific "overtaking" behavior (exclusivity grouping behavior) implementation.
	template<typename FilterType_t>
	concept ValidFilterType_c = requires(FilterType_t & t)
	{
		{ t.SetMappingRange(std::span<CBActionMap>{}) };
		{ t.GetFilteredButtonState({1,2,3}) } -> std::convertible_to<keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>>;
		{ std::movable<FilterType_t> == true };
	};

	/*
	 *	NOTE: Testing these functions may be quite easy, pass a single CBActionMap in a certain state to all of these functions,
	 *	and if more than one TranslationResult is produced (aside from perhaps the reset translation), then it would obviously be in error.
	 */

	/**
	 * \brief For a single mapping, search the controller state update buffer and produce a TranslationResult appropriate to the current mapping state and controller state.
	 * \param downKeys Wrapper class containing the results of a controller state update polling.
	 * \param singleButton The mapping type for a single virtual key of the controller.
	 * \returns Optional, <c>TranslationResult</c>
	 */
	[[nodiscard]]
	inline
	auto GetButtonTranslationForInitialToDown(const keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>& downKeys, CBActionMap& singleButton) noexcept -> std::optional<TranslationResult>
	{
		using
		std::ranges::find,
		std::ranges::end;

		if (singleButton.LastAction.IsInitialState())
		{
			const auto findResult = find(downKeys, singleButton.ButtonVirtualKeycode);
			// If VK *is* found in the down list, create the down translation.
			if(findResult != end(downKeys))
				return GetInitialKeyDownTranslationResult(singleButton);
		}
		return {};
	}

	[[nodiscard]]
	inline
	auto GetButtonTranslationForDownToRepeat(const keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>& downKeys, CBActionMap& singleButton) noexcept -> std::optional<TranslationResult>
	{
		using std::ranges::find, std::ranges::end;
		const bool isDownAndUsesRepeat = singleButton.LastAction.IsDown() && (singleButton.UsesInfiniteRepeat || singleButton.SendsFirstRepeatOnly);
		const bool isDelayElapsed = singleButton.LastAction.DelayBeforeFirstRepeat.IsElapsed();
		if (isDownAndUsesRepeat && isDelayElapsed)
		{
			const auto findResult = find(downKeys, singleButton.ButtonVirtualKeycode);
			// If VK *is* found in the down list, create the repeat translation.
			if (findResult != end(downKeys))
				return GetRepeatTranslationResult(singleButton);
		}
		return {};
	}

	[[nodiscard]]
	inline
	auto GetButtonTranslationForRepeatToRepeat(const keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>& downKeys, CBActionMap& singleButton) noexcept -> std::optional<TranslationResult>
	{
		using std::ranges::find, std::ranges::end;
		const bool isRepeatAndUsesInfinite = singleButton.LastAction.IsRepeating() && singleButton.UsesInfiniteRepeat;
		if (isRepeatAndUsesInfinite && singleButton.LastAction.LastSentTime.IsElapsed())
		{
			const auto findResult = find(downKeys, singleButton.ButtonVirtualKeycode);
			// If VK *is* found in the down list, create the repeat translation.
			if (findResult != end(downKeys))
				return GetRepeatTranslationResult(singleButton);
		}
		return {};
	}

	[[nodiscard]]
	inline
	auto GetButtonTranslationForDownOrRepeatToUp(const keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>& downKeys, CBActionMap& singleButton) noexcept -> std::optional<TranslationResult>
	{
		using std::ranges::find, std::ranges::end;
		if (singleButton.LastAction.IsDown() || singleButton.LastAction.IsRepeating())
		{
			const auto findResult = find(downKeys, singleButton.ButtonVirtualKeycode);
			// If VK is not found in the down list, create the up translation.
			if(findResult == end(downKeys))
				return GetKeyUpTranslationResult(singleButton);
		}
		return {};
	}

	// This is the reset translation
	[[nodiscard]]
	inline
	auto GetButtonTranslationForUpToInitial(CBActionMap& singleButton) noexcept -> std::optional<TranslationResult>
	{
		// if the timer has elapsed, update back to the initial state.
		if(singleButton.LastAction.IsUp() && singleButton.LastAction.LastSentTime.IsElapsed())
		{
			return GetResetTranslationResult(singleButton);
		}
		return {};
	}

	/**
	 * \brief Encapsulates the mapping buffer, processes controller state updates, returns translation packs.
	 * \remarks If, before destruction, the mappings are in a state other than initial or awaiting reset, then you may wish to
	 *	make use of the <c>GetCleanupActions()</c> function. Not copyable. Is movable.
	 *	<p></p>
	 *	<p>An invariant exists such that: <b>There must be only one mapping per virtual keycode.</b></p>
	 */
	template<ValidFilterType_c OvertakingFilter_t = KeyboardOvertakingFilter>
	class KeyboardTranslator final
	{
		using MappingVector_t = std::vector<CBActionMap>;
		static_assert(MappingRange_c<MappingVector_t>);
		MappingVector_t m_mappings;
		std::optional<OvertakingFilter_t> m_filter;
	public:
		KeyboardTranslator() = delete; // no default
		KeyboardTranslator(const KeyboardTranslator& other) = delete; // no copy
		auto operator=(const KeyboardTranslator& other)->KeyboardTranslator & = delete; // no copy-assign

		KeyboardTranslator(KeyboardTranslator&& other) = default; // move-construct
		auto operator=(KeyboardTranslator&& other) -> KeyboardTranslator& = default; // move-assign
		~KeyboardTranslator() = default;

		/**
		 * \brief Mapping Vector move Ctor, may throw on exclusivity group error, OR more than one mapping per VK.
		 * \param keyMappings Rv ref to a mapping vector type.
		 * \exception std::runtime_error on exclusivity group error during construction, OR more than one mapping per VK.
		 */
		explicit KeyboardTranslator(MappingVector_t&& keyMappings )
		: m_mappings(std::move(keyMappings))
		{
			for (auto& e : m_mappings)
				InitCustomTimers(e);
			if (!AreMappingsUniquePerVk(m_mappings) || !AreMappingVksNonZero(m_mappings))
				throw std::runtime_error("Exception: More than 1 mapping per VK!");
		}

		/**
		 * \brief Constructor used with adding a filter, note both params expect arguments std::move'd in.
		 * \param keyMappings Rv ref to a mapping vector type.
		 * \param filter Rv ref to a filter type.
		 * \exception std::runtime_error on exclusivity group error during construction, OR more than one mapping per VK.
		 */
		KeyboardTranslator(MappingVector_t&& keyMappings, OvertakingFilter_t&& filter)
			: m_mappings(std::move(keyMappings)), m_filter(std::move(filter))
		{
			for (auto& e : m_mappings)
				InitCustomTimers(e);
			if (!AreMappingsUniquePerVk(m_mappings) || !AreMappingVksNonZero(m_mappings))
				throw std::runtime_error("Exception: More than 1 mapping per VK!");
			m_filter->SetMappingRange(m_mappings);
		}
	public:
		[[nodiscard]]
		auto operator()(keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>&& stateUpdate) noexcept -> TranslationPack
		{
			return GetUpdatedState(std::move(stateUpdate));
		}

		[[nodiscard]]
		auto GetUpdatedState(keyboardtypes::SmallVector_t<keyboardtypes::VirtualKey_t>&& stateUpdate) noexcept -> TranslationPack
		{
			auto stateUpdateFiltered = m_filter.has_value() ? m_filter->GetFilteredButtonState(std::move(stateUpdate)) : std::move(stateUpdate);

			TranslationPack translations;
			for(auto& mapping : m_mappings)
			{
				if (const auto upToInitial = GetButtonTranslationForUpToInitial(mapping))
				{
					translations.UpdateRequests.emplace_back(*upToInitial);
				}
				else if (const auto initialToDown = GetButtonTranslationForInitialToDown(stateUpdateFiltered, mapping))
				{
					// Advance to next state.
					translations.DownRequests.emplace_back(*initialToDown);
				}
				else if (const auto downToFirstRepeat = GetButtonTranslationForDownToRepeat(stateUpdateFiltered, mapping))
				{
					translations.RepeatRequests.emplace_back(*downToFirstRepeat);
				}
				else if (const auto repeatToRepeat = GetButtonTranslationForRepeatToRepeat(stateUpdateFiltered, mapping))
				{
					translations.RepeatRequests.emplace_back(*repeatToRepeat);
				}
				else if (const auto repeatToUp = GetButtonTranslationForDownOrRepeatToUp(stateUpdateFiltered, mapping))
				{
					translations.UpRequests.emplace_back(*repeatToUp);
				}
			}
			return translations;
		}

		[[nodiscard]]
		auto GetCleanupActions() noexcept -> keyboardtypes::SmallVector_t<TranslationResult>
		{
			keyboardtypes::SmallVector_t<TranslationResult> translations;
			for(auto & mapping : m_mappings)
			{
				if(DoesMappingNeedCleanup(mapping.LastAction))
				{
					translations.emplace_back(GetKeyUpTranslationResult(mapping));
				}
			}
			return translations;
		}
	};

	static_assert(InputTranslator_c<KeyboardTranslator<>>);
	static_assert(std::movable<KeyboardTranslator<>>);
	static_assert(std::copyable<KeyboardTranslator<>> == false);

}