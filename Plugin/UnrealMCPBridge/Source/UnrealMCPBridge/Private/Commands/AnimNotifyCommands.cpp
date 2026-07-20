#include "Commands/AnimNotifyCommands.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

namespace
{
    UAnimSequenceBase* LoadAnim(const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError)
    {
        FString Path;
        if (!Params.IsValid() || !Params->TryGetStringField(TEXT("animation_path"), Path) || Path.IsEmpty())
        {
            OutError.Code = MCPProtocol::FMCPError::InvalidParams;
            OutError.Message = TEXT("animation_path is required and must be non-empty");
            return nullptr;
        }
        UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *Path);
        if (!Anim)
        {
            OutError.Code = MCPProtocol::FMCPError::InvalidParams;
            OutError.Message = FString::Printf(TEXT("Could not load animation: %s"), *Path);
        }
        return Anim;
    }

    // Name of whatever sits on this event, notify or notify-state, for reporting/filtering.
    FString EventClassName(const FAnimNotifyEvent& Event)
    {
        if (Event.NotifyStateClass) { return Event.NotifyStateClass->GetClass()->GetName(); }
        if (Event.Notify)           { return Event.Notify->GetClass()->GetName(); }
        return TEXT("(none)");
    }

    bool EventMatchesFilter(const FAnimNotifyEvent& Event, const FString& ClassFilter)
    {
        if (ClassFilter.IsEmpty())
        {
            return true;
        }
        const UClass* Class = Event.NotifyStateClass ? Event.NotifyStateClass->GetClass()
                            : (Event.Notify ? Event.Notify->GetClass() : nullptr);
        for (const UClass* It = Class; It; It = It->GetSuperClass())
        {
            if (It->GetName() == ClassFilter || It->GetPathName() == ClassFilter)
            {
                return true;
            }
        }
        return false;
    }
}

void FAnimNotifyCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // animnotify.list — lists notify events on an animation. Params: animation_path,
    // notify_class (optional filter, class name or path; matches subclasses too).
    // Returns { notifies: [ { index, class, start_time, duration, track } ] }.
    Registry.Register(TEXT("animnotify.list"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UAnimSequenceBase* Anim = LoadAnim(Params, OutError); if (!Anim) return nullptr;
            FString ClassFilter; Params->TryGetStringField(TEXT("notify_class"), ClassFilter);

            TArray<TSharedPtr<FJsonValue>> Out;
            for (int32 Index = 0; Index < Anim->Notifies.Num(); ++Index)
            {
                const FAnimNotifyEvent& Event = Anim->Notifies[Index];
                if (!EventMatchesFilter(Event, ClassFilter))
                {
                    continue;
                }
                auto Entry = MakeShared<FJsonObject>();
                Entry->SetNumberField(TEXT("index"), Index);
                Entry->SetStringField(TEXT("class"), EventClassName(Event));
                Entry->SetNumberField(TEXT("start_time"), Event.GetTriggerTime());
                Entry->SetNumberField(TEXT("duration"), Event.GetDuration());
                Entry->SetNumberField(TEXT("track"), Event.TrackIndex);
                Out.Add(MakeShared<FJsonValueObject>(Entry));
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetArrayField(TEXT("notifies"), Out);
            return Result;
        });

    // animnotify.set_window — retimes an existing notify event in place. Params: animation_path, index
    // (from animnotify.list), start_time (optional), duration (optional; notify states only).
    // Returns { ok, class, start_time, duration }.
    // In place on purpose: removing and re-adding the event would rebuild its Instanced sub-objects from
    // class defaults, quietly dropping whatever the animator configured on them.
    Registry.Register(TEXT("animnotify.set_window"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UAnimSequenceBase* Anim = LoadAnim(Params, OutError); if (!Anim) return nullptr;

            double IndexValue = -1.0;
            if (!Params->TryGetNumberField(TEXT("index"), IndexValue))
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("index is required (see animnotify.list)");
                return nullptr;
            }
            const int32 Index = static_cast<int32>(IndexValue);
            if (!Anim->Notifies.IsValidIndex(Index))
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("index %d out of range (%d notifies)"), Index, Anim->Notifies.Num());
                return nullptr;
            }

            FAnimNotifyEvent& Event = Anim->Notifies[Index];

            double StartTime = Event.GetTriggerTime();
            double Duration = Event.GetDuration();
            const bool bHasStart = Params->TryGetNumberField(TEXT("start_time"), StartTime);
            const bool bHasDuration = Params->TryGetNumberField(TEXT("duration"), Duration);
            if (!bHasStart && !bHasDuration)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("provide start_time and/or duration");
                return nullptr;
            }

            const float PlayLength = Anim->GetPlayLength();
            if (StartTime < 0.0 || StartTime > PlayLength || (StartTime + Duration) > PlayLength + KINDA_SMALL_NUMBER)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("window %.3f..%.3f falls outside the animation (0..%.3f)"),
                    StartTime, StartTime + Duration, PlayLength);
                return nullptr;
            }
            if (bHasDuration && !Event.NotifyStateClass)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("duration only applies to notify states; this event is a plain notify");
                return nullptr;
            }

            Anim->Modify();

            // Same sequence the editor uses when placing a notify: re-link to the new time, refresh the
            // trigger offset, then re-link the end so the state's window follows.
            Event.Link(Anim, StartTime);
            Event.RefreshTriggerOffset(Anim->CalculateOffsetForNotify(StartTime));
            if (Event.NotifyStateClass)
            {
                Event.SetDuration(Duration);
                Event.EndLink.Link(Anim, StartTime + Duration);
                Event.RefreshEndTriggerOffset(Anim->CalculateOffsetForNotify(StartTime + Duration));
            }

            Anim->RefreshCacheData();
            Anim->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("ok"), true);
            Result->SetStringField(TEXT("class"), EventClassName(Event));
            Result->SetNumberField(TEXT("start_time"), Event.GetTriggerTime());
            Result->SetNumberField(TEXT("duration"), Event.GetDuration());
            return Result;
        });
}
