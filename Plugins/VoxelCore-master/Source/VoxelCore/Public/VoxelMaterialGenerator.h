﻿// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "MaterialExpressionIO.h"

class UMaterialInstance;

#if WITH_EDITOR
class VOXELCORE_API FVoxelMaterialGenerator
{
public:
	FVoxelMaterialGenerator(
		const TVoxelObjectPtr<const UObject> ErrorOwner,
		UMaterial& NewMaterial,
		const FString& ParameterNamePrefix,
		const bool bSkipCustomOutputs,
		const TFunction<bool(const UMaterialExpression&)> ShouldDuplicateFunction_AdditionalHook)
		: ErrorOwner(ErrorOwner)
		, NewMaterial(NewMaterial)
		, ParameterNamePrefix(ParameterNamePrefix)
		, bSkipCustomOutputs(bSkipCustomOutputs)
		, ShouldDuplicateFunction_AdditionalHook(ShouldDuplicateFunction_AdditionalHook)
	{
	}

public:
	UMaterialFunction* DuplicateFunctionIfNeeded(const UMaterialFunction& OldFunction);
	TVoxelOptional<FMaterialAttributesInput> CopyExpressions(const UMaterial& OldMaterial);

public:
	FVoxelOptionalIntBox2D GetBounds() const;
	void MoveExpressions(const FIntPoint& Offset) const;

private:
	const TVoxelObjectPtr<const UObject> ErrorOwner;
	UMaterial& NewMaterial;
	const FString ParameterNamePrefix;
	const bool bSkipCustomOutputs;
	const TFunction<bool(const UMaterialExpression&)> ShouldDuplicateFunction_AdditionalHook;

	TVoxelMap<FGuid, FGuid> OldToNewParameterGuid;
	TVoxelMap<FGuid, FGuid> OldToNewNamedRerouteGuid;
	TVoxelMap<const UMaterialExpression*, UMaterialExpression*> OldToNewExpression;
	TVoxelMap<const UMaterialFunction*, UMaterialFunction*> OldToNewFunction;
	TVoxelMap<const UMaterialFunction*, bool> FunctionToShouldDuplicate;

	bool ShouldDuplicateFunction(const UMaterialFunction& Function);
	bool PostCopyExpression(UMaterialExpression& Expression);

	bool CopyFunctionExpressions(
		const UMaterialFunction& OldFunction,
		UMaterialFunction& NewFunction);

	static UMaterialExpression* CloneExpression(
		const UMaterialExpression& Expression,
		UObject& Outer);
};
#endif