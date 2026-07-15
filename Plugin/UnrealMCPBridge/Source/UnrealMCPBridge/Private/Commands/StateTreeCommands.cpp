#include "Commands/StateTreeCommands.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"

#include "StateTree.h"
#include "StateTreeState.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeTypes.h"
#include "PropertyBindingPath.h"

namespace
{
    UStateTree* LoadTree(const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError)
    {
        FString Path;
        if (!Params.IsValid() || !Params->TryGetStringField(TEXT("tree_path"), Path) || Path.IsEmpty())
        {
            OutError.Code = MCPProtocol::FMCPError::InvalidParams;
            OutError.Message = TEXT("tree_path is required and must be non-empty");
            return nullptr;
        }
        UStateTree* Tree = LoadObject<UStateTree>(nullptr, *Path);
        if (!Tree)
        {
            OutError.Code = MCPProtocol::FMCPError::InvalidParams;
            OutError.Message = FString::Printf(TEXT("Could not load StateTree: %s"), *Path);
        }
        return Tree;
    }

    UStateTreeEditorData* GetEditorData(UStateTree* Tree, MCPProtocol::FMCPError& OutError)
    {
        UStateTreeEditorData* Ed = Tree ? Cast<UStateTreeEditorData>(Tree->EditorData) : nullptr;
        if (!Ed)
        {
            OutError.Code = MCPProtocol::FMCPError::InternalError;
            OutError.Message = TEXT("StateTree has no editor data (was it created with a schema?)");
        }
        return Ed;
    }

    bool RequireString(const TSharedPtr<FJsonObject>& Params, const TCHAR* Field, FString& Out, MCPProtocol::FMCPError& OutError)
    {
        if (!Params->TryGetStringField(Field, Out) || Out.IsEmpty())
        {
            OutError.Code = MCPProtocol::FMCPError::InvalidParams;
            OutError.Message = FString::Printf(TEXT("%s is required and must be non-empty"), Field);
            return false;
        }
        return true;
    }

    // Resolves a struct path like "/Script/IFSR.STEval_SenseTarget" to a UScriptStruct.
    const UScriptStruct* FindNodeStruct(const FString& Path)
    {
        return LoadObject<UScriptStruct>(nullptr, *Path);
    }

    // Finds a state by its GUID id (string). Returns nullptr if not found.
    UStateTreeState* FindStateById(UStateTreeEditorData* Ed, const FGuid& Id)
    {
        UStateTreeState* Found = nullptr;
        Ed->VisitHierarchy([&Found, &Id](UStateTreeState& State, UStateTreeState* /*Parent*/)
        {
            if (State.ID == Id)
            {
                Found = &State;
                return EStateTreeVisitor::Break;
            }
            return EStateTreeVisitor::Continue;
        });
        return Found;
    }

    // Finds the first root subtree state (parent == nullptr).
    UStateTreeState* FindRootState(UStateTreeEditorData* Ed)
    {
        UStateTreeState* Root = nullptr;
        Ed->VisitHierarchy([&Root](UStateTreeState& State, UStateTreeState* Parent)
        {
            if (Parent == nullptr)
            {
                Root = &State;
                return EStateTreeVisitor::Break;
            }
            return EStateTreeVisitor::Continue;
        });
        return Root;
    }

    EStateTreeTransitionTrigger ParseTrigger(const FString& Str)
    {
        if (Str.Equals(TEXT("OnStateCompleted"), ESearchCase::IgnoreCase)) return EStateTreeTransitionTrigger::OnStateCompleted;
        if (Str.Equals(TEXT("OnStateSucceeded"), ESearchCase::IgnoreCase)) return EStateTreeTransitionTrigger::OnStateSucceeded;
        if (Str.Equals(TEXT("OnEvent"), ESearchCase::IgnoreCase))          return EStateTreeTransitionTrigger::OnEvent;
        return EStateTreeTransitionTrigger::OnTick; // default: evaluate every tick
    }

    // Adds an editor node (task/eval) of the given struct into an array, returns its new GUID (or invalid).
    FGuid AddNodeToArray(TArray<FStateTreeEditorNode>& Array, UObject* Outer, const UScriptStruct* Struct)
    {
        FStateTreeEditorNode& Node = Array.AddDefaulted_GetRef();
        Node.InitializeAs(Outer, Struct);
        Node.ID = FGuid::NewGuid();
        return Node.ID;
    }
}

void FStateTreeCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // statetree.add_state — adds a state. Params: tree_path, state_name, parent_state_id (optional; empty=root).
    // Returns { state_id }.
    Registry.Register(TEXT("statetree.add_state"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;
            UStateTreeEditorData* Ed = GetEditorData(Tree, OutError); if (!Ed) return nullptr;

            FString StateName; if (!RequireString(Params, TEXT("state_name"), StateName, OutError)) return nullptr;
            FString ParentId; Params->TryGetStringField(TEXT("parent_state_id"), ParentId);

            UStateTreeState* Parent = nullptr;
            if (!ParentId.IsEmpty())
            {
                FGuid Guid; FGuid::Parse(ParentId, Guid);
                Parent = FindStateById(Ed, Guid);
                if (!Parent) { OutError.Code = MCPProtocol::FMCPError::InvalidParams; OutError.Message = TEXT("parent_state_id not found"); return nullptr; }
            }
            else
            {
                Parent = FindRootState(Ed);
                if (!Parent) { Parent = &Ed->AddRootState(); }
            }

            UStateTreeState& NewState = Parent->AddChildState(FName(*StateName));
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("state_id"), NewState.ID.ToString());
            return Result;
        });

    // statetree.add_evaluator — Params: tree_path, struct_path. Returns { node_id }.
    Registry.Register(TEXT("statetree.add_evaluator"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;
            UStateTreeEditorData* Ed = GetEditorData(Tree, OutError); if (!Ed) return nullptr;

            FString StructPath; if (!RequireString(Params, TEXT("struct_path"), StructPath, OutError)) return nullptr;
            const UScriptStruct* Struct = FindNodeStruct(StructPath);
            if (!Struct) { OutError.Code = MCPProtocol::FMCPError::InvalidParams; OutError.Message = FString::Printf(TEXT("struct not found: %s"), *StructPath); return nullptr; }

            const FGuid Id = AddNodeToArray(Ed->Evaluators, Ed, Struct);
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("node_id"), Id.ToString());
            return Result;
        });

    // statetree.add_task — Params: tree_path, state_id, struct_path. Returns { node_id }.
    Registry.Register(TEXT("statetree.add_task"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;
            UStateTreeEditorData* Ed = GetEditorData(Tree, OutError); if (!Ed) return nullptr;

            FString StateId, StructPath;
            if (!RequireString(Params, TEXT("state_id"), StateId, OutError)) return nullptr;
            if (!RequireString(Params, TEXT("struct_path"), StructPath, OutError)) return nullptr;

            FGuid Guid; FGuid::Parse(StateId, Guid);
            UStateTreeState* State = FindStateById(Ed, Guid);
            if (!State) { OutError.Code = MCPProtocol::FMCPError::InvalidParams; OutError.Message = TEXT("state_id not found"); return nullptr; }

            const UScriptStruct* Struct = FindNodeStruct(StructPath);
            if (!Struct) { OutError.Code = MCPProtocol::FMCPError::InvalidParams; OutError.Message = FString::Printf(TEXT("struct not found: %s"), *StructPath); return nullptr; }

            const FGuid Id = AddNodeToArray(State->Tasks, Ed, Struct);
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("node_id"), Id.ToString());
            return Result;
        });

    // statetree.add_transition — Params: tree_path, from_state_id, to_state_id, trigger (default OnTick).
    Registry.Register(TEXT("statetree.add_transition"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;
            UStateTreeEditorData* Ed = GetEditorData(Tree, OutError); if (!Ed) return nullptr;

            FString FromId, ToId;
            if (!RequireString(Params, TEXT("from_state_id"), FromId, OutError)) return nullptr;
            if (!RequireString(Params, TEXT("to_state_id"), ToId, OutError)) return nullptr;
            FString TriggerStr; Params->TryGetStringField(TEXT("trigger"), TriggerStr);

            FGuid FromGuid, ToGuid; FGuid::Parse(FromId, FromGuid); FGuid::Parse(ToId, ToGuid);
            UStateTreeState* From = FindStateById(Ed, FromGuid);
            UStateTreeState* To = FindStateById(Ed, ToGuid);
            if (!From || !To) { OutError.Code = MCPProtocol::FMCPError::InvalidParams; OutError.Message = TEXT("from/to state not found"); return nullptr; }

            FStateTreeTransition& Transition = From->AddTransition(ParseTrigger(TriggerStr), EStateTreeTransitionType::GotoState, To);
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("transition_id"), Transition.ID.ToString());
            return Result;
        });

    // statetree.add_binding — binds an output of source node to an input of target node.
    // Params: tree_path, source_node_id, source_path, target_node_id, target_path.
    Registry.Register(TEXT("statetree.add_binding"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;
            UStateTreeEditorData* Ed = GetEditorData(Tree, OutError); if (!Ed) return nullptr;

            FString SrcId, SrcPath, TgtId, TgtPath;
            if (!RequireString(Params, TEXT("source_node_id"), SrcId, OutError)) return nullptr;
            if (!RequireString(Params, TEXT("source_path"), SrcPath, OutError)) return nullptr;
            if (!RequireString(Params, TEXT("target_node_id"), TgtId, OutError)) return nullptr;
            if (!RequireString(Params, TEXT("target_path"), TgtPath, OutError)) return nullptr;

            FGuid SrcGuid, TgtGuid; FGuid::Parse(SrcId, SrcGuid); FGuid::Parse(TgtId, TgtGuid);

            FPropertyBindingPath Source; Source.SetStructID(SrcGuid);
            FPropertyBindingPath Target; Target.SetStructID(TgtGuid);
            if (!Source.FromString(SrcPath) || !Target.FromString(TgtPath))
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("invalid source/target property path");
                return nullptr;
            }
            Ed->AddPropertyBinding(Source, Target);
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("ok"), true);
            return Result;
        });

    // statetree.compile — compiles the StateTree from its editor data. Params: tree_path.
    Registry.Register(TEXT("statetree.compile"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            UStateTree* Tree = LoadTree(Params, OutError); if (!Tree) return nullptr;

            FStateTreeCompilerLog Log;
            const bool bOk = UStateTreeEditingSubsystem::CompileStateTree(Tree, Log);
            Tree->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("compiled"), bOk);
            return Result;
        });
}
