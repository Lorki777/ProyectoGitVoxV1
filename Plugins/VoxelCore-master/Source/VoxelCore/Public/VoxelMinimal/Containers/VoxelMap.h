// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/Containers/VoxelSet.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"

// Minimize padding by using the best of all possible permutation between Key, Value and NextElementIndex
// In practice we only need to check two permutations:
// - Key Value NextElementIndex: possible merges: KeyValue, ValueNextElementIndex
// - Value Key NextElementIndex: possible merges: KeyValue, KeyNextElementIndex
template<typename KeyType, typename ValueType>
struct TVoxelMapElementBase
{
private:
	struct FElementKeyValue
	{
		const KeyType Key;
		ValueType Value;
		int32 NextElementIndex VOXEL_DEBUG_ONLY(= -16);

		FElementKeyValue() = default;

		template<typename InValueType>
		FORCEINLINE FElementKeyValue(
			const KeyType& Key,
			InValueType&& Value)
			: Key(Key)
			, Value(Forward<InValueType>(Value))
		{
		}
	};
	struct FElementValueKey
	{
		ValueType Value;
		const KeyType Key;
		int32 NextElementIndex VOXEL_DEBUG_ONLY(= -16);

		FElementValueKey() = default;

		template<typename InValueType>
		FORCEINLINE FElementValueKey(
			const KeyType& Key,
			InValueType&& Value)
			: Value(Forward<InValueType>(Value))
			, Key(Key)
		{
		}
	};

public:
	using FElement = std::conditional_t<sizeof(FElementKeyValue) <= sizeof(FElementValueKey), FElementKeyValue, FElementValueKey>;
};

template<typename KeyType, typename ValueType>
struct TVoxelMapElement : TVoxelMapElementBase<KeyType, ValueType>::FElement
{
public:
	using Super = typename TVoxelMapElementBase<KeyType, ValueType>::FElement;

	using Super::Key;
	using Super::Value;

private:
	using Super::NextElementIndex;
	using Super::Super;

	FORCEINLINE bool KeyEquals(const KeyType& OtherKey) const
	{
		return Key == OtherKey;
	}
	FORCEINLINE void MoveFrom(TVoxelMapElement&& Other)
	{
		const_cast<KeyType&>(Key) = MoveTemp(const_cast<KeyType&>(Other.Key));
		Value = MoveTemp(Other.Value);
		NextElementIndex = Other.NextElementIndex;
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TVoxelMapElement& Element)
	{
		Ar << const_cast<KeyType&>(Element.Key);
		Ar << Element.Value;
		return Ar;
	}

	template<typename, typename, typename>
	friend class TVoxelMap;
};

template<typename KeyType, typename ValueType>
struct TVoxelDefaultMapAllocator
{
	static constexpr int32 MinHashSize = 0;

	using FHashArray = TVoxelArray<int32>;
	using FElementArray = TVoxelArray<TVoxelMapElement<KeyType, ValueType>>;
};

// Simple map with an array of elements and a hash table
// The array isn't sparse, so removal will not keep order (it's basically a RemoveSwap)
//
// In a shipping build:
// TVoxelMap::FindChecked   1.1x faster
// TVoxelMap::Remove        1.2x faster
// TVoxelMap::Reserve(1M)  74.4x faster
// TVoxelMap::FindOrAdd     2.2x faster
// TVoxelMap::Add_CheckNew  4.0x faster
template<typename KeyType, typename ValueType, typename Allocator = TVoxelDefaultMapAllocator<KeyType, ValueType>>
class TVoxelMap
{
public:
	using FElement = TVoxelMapElement<KeyType, ValueType>;

	TVoxelMap() = default;
	TVoxelMap(const TVoxelMap&) = default;
	TVoxelMap& operator=(const TVoxelMap&) = default;

	TVoxelMap(TVoxelMap&& Other)
		: HashTable(MoveTemp(Other.HashTable))
		, Elements(MoveTemp(Other.Elements))
	{
		Other.Reset();
	}
	TVoxelMap& operator=(TVoxelMap&& Other)
	{
		HashTable = MoveTemp(Other.HashTable);
		Elements = MoveTemp(Other.Elements);
		Other.Reset();
		return *this;
	}

	template<typename OtherKeyType, typename OtherValueType, typename OtherAllocator>
	requires
	(
		!std::is_same_v<TVoxelMap, TVoxelMap<OtherKeyType, OtherValueType, OtherAllocator>> &&
		std::is_constructible_v<KeyType, OtherKeyType> &&
		std::is_constructible_v<ValueType, OtherValueType>
	)
	explicit TVoxelMap(const TVoxelMap<OtherKeyType, OtherValueType, OtherAllocator>& Other)
	{
		this->Append(Other);
	}

public:
	TVoxelMap(std::initializer_list<TPairInitializer<const KeyType&, const ValueType&>> Initializer)
	{
		this->Reserve(Initializer.size());

		for (const TPairInitializer<const KeyType&, const ValueType&>& Element : Initializer)
		{
			this->FindOrAdd(Element.Key) = Element.Value;
		}
	}

public:
	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return HashTable.GetAllocatedSize() + Elements.GetAllocatedSize();
	}
	FORCEINLINE TVoxelArrayView<FElement> GetElements()
	{
		return Elements;
	}
	FORCEINLINE TConstVoxelArrayView<FElement> GetElements() const
	{
		return Elements;
	}

	void Reset()
	{
		Elements.Reset();
		HashTable.Reset();
	}
	void Reset_KeepHashSize()
	{
		Elements.Reset();
		Rehash();
	}
	void Empty()
	{
		Elements.Empty();
		HashTable.Empty();
	}
	void Shrink()
	{
		VOXEL_FUNCTION_COUNTER();

		if (Num() == 0)
		{
			// Needed as GetHashSize(0) = 1
			Empty();
			return;
		}

		if (HashTable.Num() != GetHashSize(Num()))
		{
			checkVoxelSlow(HashTable.Num() > GetHashSize(Num()));

			HashTable.Reset();
			Rehash();
		}

		HashTable.Shrink();
		Elements.Shrink();
	}
	void Reserve(const int32 Number)
	{
		if (Number <= Elements.Num())
		{
			return;
		}

		VOXEL_FUNCTION_COUNTER_NUM(Number, 1024);

		Elements.Reserve(Number);

		const int32 NewHashSize = GetHashSize(Number);
		if (HashTable.Num() < NewHashSize)
		{
			FVoxelUtilities::SetNumFast(HashTable, NewHashSize);
			Rehash();
		}
	}
	void ReserveGrow(const int32 Number)
	{
		Reserve(Num() + Number);
	}

	template<typename OtherAllocator>
	bool OrderIndependentEqual(const TVoxelMap<KeyType, ValueType, OtherAllocator>& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (const auto& It : Other)
		{
			const ValueType* Value = this->Find(It.Key);
			if (!Value)
			{
				return false;
			}
			if (!(*Value == It.Value))
			{
				return false;
			}
		}
		return true;
	}
	template<typename OtherAllocator>
	bool OrderDependentEqual(const TVoxelMap<KeyType, ValueType, OtherAllocator>& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Num(); Index++)
		{
			const FElement& Element = Elements[Index];
			const FElement& OtherElement = Other.Elements[Index];

			if (!(Element.Key == OtherElement.Key) ||
				!(Element.Value == OtherElement.Value))
			{
				return false;
			}
		}

		return true;
	}

	template<typename OtherValueType, typename OtherAllocator>
	bool HasSameKeys(const TVoxelMap<KeyType, OtherValueType, OtherAllocator>& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (const auto& It : Other)
		{
			if (!this->Contains(It.Key))
			{
				return false;
			}
		}
		return true;
	}

	template<typename OtherKeyType, typename OtherValueType, typename OtherAllocator>
	requires
	(
		std::is_constructible_v<KeyType, OtherKeyType> &&
		std::is_constructible_v<ValueType, OtherValueType>
	)
	void Append(const TVoxelMap<OtherKeyType, OtherValueType, OtherAllocator>& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		if (std::is_same_v<KeyType, OtherKeyType> &&
			Num() == 0)
		{
			// We can reuse the hash table

			HashTable = Other.HashTable;

			Elements.Reserve(Other.Elements.Num());

			for (const typename TVoxelMap<OtherKeyType, OtherValueType, OtherAllocator>::FElement& Element : Other.Elements)
			{
				Elements.Emplace_CheckNoGrow(KeyType(Element.Key), ValueType(Element.Value));
			}

			return;
		}

		this->ReserveGrow(Other.Num());

		for (const auto& It : Other)
		{
			const KeyType Key = KeyType(It.Key);
			const uint32 Hash = this->HashValue(Key);

			if (ValueType* Value = this->FindHashed(Hash, Key))
			{
				*Value = ValueType(It.Value);
			}
			else
			{
				this->AddHashed_CheckNew_CheckNoRehash(Hash, Key, It.Value);
			}
		}
	}
	TVoxelArray<KeyType> KeyArray() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelArray<KeyType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add_CheckNoGrow(Element.Key);
		}
		return Result;
	}
	TVoxelArray<ValueType> ValueArray() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelArray<ValueType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add_CheckNoGrow(Element.Value);
		}
		return Result;
	}

	TVoxelSet<KeyType> KeySet() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelSet<KeyType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add_CheckNew(Element.Key);
		}
		return Result;
	}
	TVoxelSet<ValueType> ValueSet() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelSet<ValueType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add(Element.Value);
		}
		return Result;
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelMap& Map)
	{
		Ar << Map.Elements;

		if (Ar.IsLoading())
		{
			Map.Rehash();
		}

		Map.CheckInvariants();
		return Ar;
	}

public:
	FORCEINLINE ValueType* Find(const KeyType& Key)
	{
		return this->FindHashed(this->HashValue(Key), Key);
	}
	FORCEINLINE ValueType* FindHashed(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(this->HashValue(Key) == Hash);
		CheckInvariants();

		if (HashTable.Num() == 0)
		{
			return nullptr;
		}

		int32 ElementIndex = this->GetElementIndex(Hash);
		while (true)
		{
			if (ElementIndex == -1)
			{
				return nullptr;
			}

			FElement& Element = Elements[ElementIndex];
			if (!Element.KeyEquals(Key))
			{
				ElementIndex = Element.NextElementIndex;
				continue;
			}

			return &Element.Value;
		}
	}
	FORCEINLINE const ValueType* Find(const KeyType& Key) const
	{
		return ConstCast(this)->Find(Key);
	}

	FORCEINLINE ValueType FindRef(const KeyType& Key) const
	{
		checkStatic(
			std::is_trivially_destructible_v<ValueType> ||
			TIsTWeakPtr_V<ValueType> ||
			TIsTSharedPtr_V<ValueType> ||
			// Hack to detect TSharedPtr wrappers like FVoxelFuture
			sizeof(ValueType) == sizeof(FSharedVoidPtr));

		if (const ValueType* Value = this->Find(Key))
		{
			return *Value;
		}
		return ValueType();
	}
	FORCEINLINE auto* FindSharedPtr(const KeyType& Key) const
	{
		if (const ValueType* Value = this->Find(Key))
		{
			return Value->Get();
		}
		return nullptr;
	}

	FORCEINLINE ValueType& FindChecked(const KeyType& Key)
	{
		checkVoxelSlow(this->Contains(Key));
		CheckInvariants();

		int32 ElementIndex = this->GetElementIndex(this->HashValue(Key));
		while (true)
		{
			checkVoxelSlow(ElementIndex != -1);

			FElement& Element = Elements[ElementIndex];
			if (!Element.KeyEquals(Key))
			{
				ElementIndex = Element.NextElementIndex;
				continue;
			}

			return Element.Value;
		}
	}
	FORCEINLINE const ValueType& FindChecked(const KeyType& Key) const
	{
		return ConstCast(this)->FindChecked(Key);
	}

	FORCEINLINE bool Contains(const KeyType& Key) const
	{
		return this->Find(Key) != nullptr;
	}

	FORCEINLINE ValueType& operator[](const KeyType& Key)
	{
		return this->FindChecked(Key);
	}
	FORCEINLINE const ValueType& operator[](const KeyType& Key) const
	{
		return this->FindChecked(Key);
	}

public:
	template<typename InKeyType>
	requires
	(
		std::is_convertible_v<const InKeyType&, KeyType> &&
		FVoxelUtilities::CanMakeSafe<ValueType>
	)
	FORCEINLINE ValueType& FindOrAdd(const InKeyType& Key)
	{
		const uint32 Hash = this->HashValue(Key);

		if (ValueType* Value = this->FindHashed(Hash, Key))
		{
			return *Value;
		}

		return this->AddHashed_CheckNew(Hash, Key, FVoxelUtilities::MakeSafe<ValueType>());
	}

public:
	// Will crash if Key is already in the map
	// 2x faster than FindOrAdd
	template<typename InKeyType>
	requires
	(
		std::is_convertible_v<const InKeyType&, KeyType> &&
		FVoxelUtilities::CanMakeSafe<ValueType>
	)
	FORCEINLINE ValueType& Add_CheckNew(const InKeyType& Key)
	{
		return this->Add_CheckNew(Key, FVoxelUtilities::MakeSafe<ValueType>());
	}
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& Add_CheckNew(const KeyType& Key, InValueType&& Value)
	{
		return this->AddHashed_CheckNew(this->HashValue(Key), Key, Forward<InValueType>(Value));
	}

public:
	template<typename InKeyType>
	requires
	(
		std::is_convertible_v<const InKeyType&, KeyType> &&
		FVoxelUtilities::CanMakeSafe<ValueType>
	)
	FORCEINLINE ValueType& Add_EnsureNew(const InKeyType& Key)
	{
		return this->Add_EnsureNew(Key, FVoxelUtilities::MakeSafe<ValueType>());
	}
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& Add_EnsureNew(const KeyType& Key, InValueType&& Value)
	{
		return this->AddHashed_EnsureNew(HashValue(Key), Key, Forward<InValueType>(Value));
	}

public:
	template<typename InKeyType>
	requires
	(
		std::is_convertible_v<const InKeyType&, KeyType> &&
		FVoxelUtilities::CanMakeSafe<ValueType>
	)
	FORCEINLINE ValueType& Add_CheckNew_CheckNoRehash(const KeyType& Key)
	{
		return this->Add_CheckNew_CheckNoRehash(Key, FVoxelUtilities::MakeSafe<ValueType>());
	}
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& Add_CheckNew_CheckNoRehash(const KeyType& Key, InValueType&& Value)
	{
		return this->AddHashed_CheckNew_CheckNoRehash(HashValue(Key), Key, Forward<InValueType>(Value));
	}

public:
	template<typename PredicateType>
	FORCENOINLINE void Sort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A, B);
		});

		Rehash();
	}
	template<typename PredicateType>
	FORCENOINLINE void KeySort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Key, B.Key);
		});

		Rehash();
	}
	template<typename PredicateType>
	FORCENOINLINE void ValueSort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Value, B.Value);
		});

		Rehash();
	}

	template<typename PredicateType>
	FORCENOINLINE bool AreKeySorted(const PredicateType& Predicate) const
	{
		for (int32 Index = 1; Index < Elements.Num(); Index++)
		{
			if (!Predicate(Elements[Index - 1].Key, Elements[Index].Key))
			{
				return false;
			}
		}
		return true;
	}
	template<typename PredicateType>
	FORCENOINLINE bool AreValueSorted(const PredicateType& Predicate) const
	{
		for (int32 Index = 1; Index < Elements.Num(); Index++)
		{
			if (!Predicate(Elements[Index - 1].Value, Elements[Index].Value))
			{
				return false;
			}
		}
		return true;
	}

public:
	void KeySort()
	{
		this->KeySort(TLess<KeyType>());
	}
	void ValueSort()
	{
		this->ValueSort(TLess<ValueType>());
	}

	bool AreKeySorted() const
	{
		return this->AreKeySorted(TLess<KeyType>());
	}
	bool AreValueSorted() const
	{
		return this->AreValueSorted(TLess<ValueType>());
	}

public:
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& AddHashed_EnsureNew(const uint32 Hash, const KeyType& Key, InValueType&& Value)
	{
		checkVoxelSlow(this->HashValue(Key) == Hash);

		if (ValueType* ExistingValue = this->FindHashed(Hash, Key))
		{
			ensure(false);
			return *ExistingValue;
		}

		return this->AddHashed_CheckNew(Hash, Key, Forward<InValueType>(Value));
	}
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& AddHashed_CheckNew(const uint32 Hash, const KeyType& Key, InValueType&& Value)
	{
		checkVoxelSlow(!this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);
		CheckInvariants();

		const int32 NewElementIndex = Elements.Emplace(Key, Forward<InValueType>(Value));
		FElement& Element = Elements[NewElementIndex];

		if (HashTable.Num() < GetHashSize(Elements.Num()))
		{
			Rehash();
		}
		else
		{
			int32& ElementIndex = this->GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return Element.Value;
	}
	template<typename InValueType>
	requires std::is_constructible_v<ValueType, InValueType&&>
	FORCEINLINE ValueType& AddHashed_CheckNew_CheckNoRehash(const uint32 Hash, const KeyType& Key, InValueType&& Value)
	{
		checkVoxelSlow(!this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);
		CheckInvariants();

		const int32 NewElementIndex = Elements.Emplace_CheckNoGrow(Key, Forward<InValueType>(Value));
		FElement& Element = Elements[NewElementIndex];

		checkVoxelSlow(GetHashSize(Elements.Num()) <= HashTable.Num());

		int32& ElementIndex = this->GetElementIndex(Hash);
		Element.NextElementIndex = ElementIndex;
		ElementIndex = NewElementIndex;

		return Element.Value;
	}
	FORCEINLINE ValueType& AddHashed_CheckNew_EnsureNoRehash(const uint32 Hash, const KeyType& Key)
	{
		ensureVoxelSlow(GetHashSize(Elements.Num()) <= HashTable.Num());
		return this->AddHashed_CheckNew(Hash, Key);
	}

public:
	// Not order-preserving
	FORCEINLINE bool RemoveAndCopyValue(const KeyType& Key, ValueType& OutRemovedValue)
	{
		const uint32 Hash = this->HashValue(Key);

		ValueType* Value = this->FindHashed(Hash, Key);
		if (!Value)
		{
			return false;
		}
		OutRemovedValue = MoveTemp(*Value);

		this->RemoveHashedChecked(Hash, Key);
		return true;
	}

	// Not order-preserving
	FORCEINLINE bool Remove(const KeyType& Key)
	{
		const uint32 Hash = this->HashValue(Key);
		if (!this->FindHashed(Hash, Key))
		{
			return false;
		}

		this->RemoveHashedChecked(Hash, Key);
		return true;
	}
	FORCEINLINE void RemoveChecked(const KeyType& Key)
	{
		this->RemoveHashedChecked(this->HashValue(Key), Key);
	}
	FORCEINLINE void RemoveHashedChecked(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);
		CheckInvariants();

		// Find element index, removing any reference to it
		int32 ElementIndex;
		{
			int32* ElementIndexPtr = &this->GetElementIndex(Hash);
			while (true)
			{
				FElement& Element = Elements[*ElementIndexPtr];
				if (!Element.KeyEquals(Key))
				{
					ElementIndexPtr = &Element.NextElementIndex;
					continue;
				}

				ElementIndex = *ElementIndexPtr;
				*ElementIndexPtr = Element.NextElementIndex;
				break;
			}
		}
		checkVoxelSlow(Elements[ElementIndex].KeyEquals(Key));

		// If we're the last element just pop
		if (ElementIndex == Elements.Num() - 1)
		{
			Elements.Pop();
			return;
		}

		// Otherwise move the last element to our index

		const KeyType LastKey = Elements.Last().Key;
		const uint32 LastHash = this->HashValue(LastKey);

		int32* ElementIndexPtr = &this->GetElementIndex(LastHash);
		while (*ElementIndexPtr != Elements.Num() - 1)
		{
			ElementIndexPtr = &Elements[*ElementIndexPtr].NextElementIndex;
		}

		*ElementIndexPtr = ElementIndex;
		Elements[ElementIndex].MoveFrom(Elements.Pop());
	}

public:
	template<bool bConst>
	struct TIterator
	{
		template<typename T>
		using TType = std::conditional_t<bConst, const T, T>;

		TType<TVoxelMap>* MapPtr = nullptr;
		TType<FElement>* ElementPtr = nullptr;
		int32 Index = 0;

		TIterator() = default;
		FORCEINLINE explicit TIterator(TType<TVoxelMap>& Map)
			: MapPtr(&Map)
		{
			if (Map.Elements.Num() > 0)
			{
				ElementPtr = &Map.Elements[0];
			}
		}

		FORCEINLINE TIterator& operator++()
		{
			Index++;
			if (Index < MapPtr->Elements.Num())
			{
				ElementPtr = &MapPtr->Elements[Index];
			}
			else
			{
				ElementPtr = nullptr;
			}
			return *this;
		}
		FORCEINLINE explicit operator bool() const
		{
			return ElementPtr != nullptr;
		}
		FORCEINLINE TType<FElement>& operator*() const
		{
			checkVoxelSlow(ElementPtr);
			return *ElementPtr;
		}
		FORCEINLINE TType<FElement>* operator->() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr;
		}
		FORCEINLINE bool operator!=(const TIterator&) const
		{
			return ElementPtr != nullptr;
		}

		FORCEINLINE const KeyType& Key() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Key;
		}
		FORCEINLINE TType<ValueType>& Value() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Value;
		}

		FORCEINLINE void RemoveCurrent()
		{
			MapPtr->RemoveChecked(MakeCopy(Key()));
			// Check for invalid access
			ElementPtr = nullptr;
			Index--;
		}
	};
	using FIterator = TIterator<false>;
	using FConstIterator = TIterator<true>;

	FORCEINLINE FIterator CreateIterator()
	{
		return FIterator(*this);
	}
	FORCEINLINE FConstIterator CreateIterator() const
	{
		return FConstIterator(*this);
	}

	FORCEINLINE FIterator begin()
	{
		return CreateIterator();
	}
	FORCEINLINE FIterator end()
	{
		return {};
	}

	FORCEINLINE FConstIterator begin() const
	{
		return CreateIterator();
	}
	FORCEINLINE FConstIterator end() const
	{
		return {};
	}

public:
	FORCEINLINE static uint32 HashValue(const KeyType& Key)
	{
		return FVoxelUtilities::HashValue(Key);
	}

private:
	typename Allocator::FHashArray HashTable;
	typename Allocator::FElementArray Elements;

	FORCEINLINE static int32 GetHashSize(const int32 NumElements)
	{
		int32 NewHashSize = FVoxelUtilities::GetHashTableSize(NumElements);

		if constexpr (Allocator::MinHashSize != 0)
		{
			NewHashSize = FMath::Max(NewHashSize, Allocator::MinHashSize);
		}

		return NewHashSize;
	}
	FORCEINLINE void CheckInvariants() const
	{
		if (Elements.Num() > 0)
		{
			checkVoxelSlow(HashTable.Num() >= GetHashSize(Elements.Num()));
		}
	}

	FORCEINLINE int32& GetElementIndex(const uint32 Hash)
	{
		const int32 HashSize = HashTable.Num();
		checkVoxelSlow(HashSize != 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(HashSize));
		return HashTable[Hash & (HashSize - 1)];
	}
	FORCEINLINE const int32& GetElementIndex(const uint32 Hash) const
	{
		return ConstCast(this)->GetElementIndex(Hash);
	}

	FORCENOINLINE void Rehash()
	{
		VOXEL_FUNCTION_COUNTER_NUM(Elements.Num(), 1024);

		const int32 NewHashSize = FMath::Max(HashTable.Num(), GetHashSize(Elements.Num()));
		checkVoxelSlow(NewHashSize >= 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(NewHashSize));

		HashTable.Reset();

		FVoxelUtilities::SetNumFast(HashTable, NewHashSize);
		FVoxelUtilities::Memset(HashTable, 0xFF);

		for (int32 Index = 0; Index < Elements.Num(); Index++)
		{
			FElement& Element = Elements[Index];

			int32& ElementIndex = this->GetElementIndex(this->HashValue(Element.Key));
			Element.NextElementIndex = ElementIndex;
			ElementIndex = Index;
		}
	}

	template<typename, typename, typename>
	friend class TVoxelMap;
};

template<typename KeyType, typename ValueType, int32 NumInlineElements>
struct TVoxelInlineMapAllocator
{
	static constexpr int32 MinHashSize = FVoxelUtilities::GetHashTableSize<NumInlineElements>();

	using FHashArray = TVoxelInlineArray<int32, MinHashSize>;
	using FElementArray = TVoxelInlineArray<TVoxelMapElement<KeyType, ValueType>, NumInlineElements>;
};

template<typename KeyType, typename ValueType, int32 NumInlineElements>
using TVoxelInlineMap = TVoxelMap<KeyType, ValueType, TVoxelInlineMapAllocator<KeyType, ValueType, NumInlineElements>>;