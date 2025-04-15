// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"
#include "VoxelPromiseState.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelEnablePromiseTracking, false,
	"voxel.EnablePromiseTracking",
	"");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<IVoxelPromiseState> IVoxelPromiseState::New(
	FVoxelTaskContext* ContextOverride,
	const bool bWithValue)
{
	return MakeShared<FVoxelPromiseState>(ContextOverride, bWithValue);
}

TSharedRef<IVoxelPromiseState> IVoxelPromiseState::New(const FSharedVoidRef& Value)
{
	return MakeShared<FVoxelPromiseState>(Value);
}

void IVoxelPromiseState::Set()
{
	static_cast<FVoxelPromiseState*>(this)->Set();
}

void IVoxelPromiseState::Set(const FSharedVoidRef& NewValue)
{
	static_cast<FVoxelPromiseState*>(this)->Set(NewValue);
}

void IVoxelPromiseState::AddContinuation(const FVoxelFuture& Future)
{
	static_cast<FVoxelPromiseState&>(*this).AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Future));
}

void IVoxelPromiseState::AddContinuation(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Continuation)
{
	static_cast<FVoxelPromiseState*>(this)->AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Thread, MoveTemp(Continuation)));
}

void IVoxelPromiseState::AddContinuation(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void(const FSharedVoidRef&)> Continuation)
{
	static_cast<FVoxelPromiseState*>(this)->AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Thread, MoveTemp(Continuation)));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture::FVoxelFuture(const TConstVoxelArrayView<FVoxelFuture> Futures)
{
	VOXEL_FUNCTION_COUNTER_NUM(Futures.Num(), 16);

	if (Futures.Num() == 0)
	{
		return;
	}

	PromiseState = IVoxelPromiseState::New(nullptr, false);

	const TSharedRef<FVoxelCounter32> Counter = MakeShared<FVoxelCounter32>(Futures.Num());

	for (const FVoxelFuture& Future : Futures)
	{
		if (Future.IsComplete())
		{
			if (Counter->Decrement_ReturnNew() == 0)
			{
				PromiseState->Set();
			}
			continue;
		}

		Future.PromiseState->AddContinuation(EVoxelFutureThread::AnyThread, [Counter, PromiseState = PromiseState]
		{
			if (Counter->Decrement_ReturnNew() == 0)
			{
				PromiseState->Set();
			}
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelFuture::ExecuteImpl(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
	FVoxelTaskScope::GetContext().Dispatch(Thread, MoveTemp(Lambda));
}